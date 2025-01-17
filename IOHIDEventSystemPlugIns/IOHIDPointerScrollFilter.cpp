//
//  IOHIDPointerScrollFilter.cpp
//  IOHIDFamily
//
//  Created by Yevgen Goryachok on 10/30/15.
//
//

#include <AssertMacros.h>
#include "IOHIDPointerScrollFilter.h"


#include <new>
#include <TargetConditionals.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFLogUtilities.h>
#include <IOKit/hid/IOHIDServiceFilterPlugIn.h>
#include <IOKit/hid/IOHIDEventSystemPrivate.h>
#include <IOKit/hid/IOHIDEventTypes.h>
#include <IOKit/hid/IOHIDSession.h>

#include <IOKit/hid/IOHIDServiceClient.h>
#include <IOKit/hid/IOHIDPrivateKeys.h>
#include <IOKit/hid/IOHIDEventSystemKeys.h>
#include <IOKit/hid/AppleHIDUsageTables.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDServiceKeys.h>
#include <IOKit/hid/IOHIDEventServiceTypes.h>
#include <IOKit/hid/IOHIDLibPrivate.h>
#include  "IOHIDParameter.h"
#include  "IOHIDEventData.h"

#include <notify.h>
#include <pthread.h>
#include <asl.h>
#include <fcntl.h>
#include <cmath>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include "CF.h"
#include "IOHIDevicePrivateKeys.h"
#include <sstream>
#include "IOHIDPrivateKeys.h"


#define SERVICE_ID (_service ? IOHIDServiceGetRegistryID(_service) : NULL)


#define LEGACY_SHIM_SCROLL_DELTA_MULTIPLIER   0.1
#define LEGACY_SHIM_POINTER_DELTA_MULTIPLIER  1

//736169DC-A8BC-45B4-BC14-645B5526E585
#define kIOHIDPointerScrollFilterFactory CFUUIDGetConstantUUIDWithBytes(kCFAllocatorSystemDefault, 0x73, 0x61, 0x69, 0xDC, 0xA8, 0xBC, 0x45, 0xB4, 0xBC, 0x14, 0x64, 0x5B, 0x55, 0x26, 0xE5, 0x85)

extern "C" void * IOHIDPointerScrollFilterFactory(CFAllocatorRef allocator, CFUUIDRef typeUUID);


static const UInt8 defaultAccelTable[] = {
    0x00, 0x00, 0x80, 0x00,
    0x40, 0x32, 0x30, 0x30, 0x00, 0x02, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x09, 0x00, 0x00, 0x71, 0x3B, 0x00, 0x00,
    0x60, 0x00, 0x00, 0x04, 0x4E, 0xC5, 0x00, 0x10,
    0x80, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x5F,
    0x00, 0x00, 0x00, 0x16, 0xEC, 0x4F, 0x00, 0x8B,
    0x00, 0x00, 0x00, 0x1D, 0x3B, 0x14, 0x00, 0x94,
    0x80, 0x00, 0x00, 0x22, 0x76, 0x27, 0x00, 0x96,
    0x00, 0x00, 0x00, 0x24, 0x62, 0x76, 0x00, 0x96,
    0x00, 0x00, 0x00, 0x26, 0x00, 0x00, 0x00, 0x96,
    0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x96,
    0x00, 0x00
};


//------------------------------------------------------------------------------
// IOHIDPointerScrollFilterFactory
//------------------------------------------------------------------------------

void *IOHIDPointerScrollFilterFactory(CFAllocatorRef allocator __unused, CFUUIDRef typeUUID)
{
    if (CFEqual(typeUUID, kIOHIDServiceFilterPlugInTypeID)) {
        void *p = CFAllocatorAllocate(kCFAllocatorDefault, sizeof(IOHIDPointerScrollFilter), 0);
        return new(p) IOHIDPointerScrollFilter(kIOHIDPointerScrollFilterFactory);
    }

    return NULL;
}

// The IOHIDPointerScrollFilter function table.
IOHIDServiceFilterPlugInInterface IOHIDPointerScrollFilter::sIOHIDPointerScrollFilterFtbl =
{
    // Required padding for COM
    NULL,
    // These three are the required COM functions
    IOHIDPointerScrollFilter::QueryInterface,
    IOHIDPointerScrollFilter::AddRef,
    IOHIDPointerScrollFilter::Release,
    // IOHIDSimpleServiceFilterPlugInInterface functions
    IOHIDPointerScrollFilter::match,
    IOHIDPointerScrollFilter::filter,
    NULL,
    // IOHIDServiceFilterPlugInInterface functions
    IOHIDPointerScrollFilter::open,
    IOHIDPointerScrollFilter::close,
    IOHIDPointerScrollFilter::scheduleWithDispatchQueue,
    IOHIDPointerScrollFilter::unscheduleFromDispatchQueue,
    IOHIDPointerScrollFilter::copyPropertyForClient,
    IOHIDPointerScrollFilter::setPropertyForClient,
    NULL,
    IOHIDPointerScrollFilter::setEventCallback,
};

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::IOHIDPointerScrollFilter
//------------------------------------------------------------------------------
IOHIDPointerScrollFilter::IOHIDPointerScrollFilter(CFUUIDRef factoryID):
  _serviceInterface(&sIOHIDPointerScrollFilterFtbl),
  _factoryID(NULL),
  _refCount(1),
  _matchScore(0),
  _eventCallback(NULL),
  _eventTarget(NULL),
  _eventContext(NULL),
  _pointerAccelerator(NULL),
  _queue(NULL),
  _cachedProperty (0),
  _service(NULL),
  _pointerAcceleration(-1),
  _scrollAcceleration(-1),
  _leagacyShim(false),
  _pointerAccelerationSupported(true),
  _scrollAccelerationSupported(true),
  _scrollMomentumMult(1.0)
{
  for (size_t index = 0; index < sizeof(_scrollAccelerators)/sizeof(_scrollAccelerators[0]); index++) {
    _scrollAccelerators[index] = NULL;
  }
  if (factoryID) {
    _factoryID = static_cast<CFUUIDRef>(CFRetain(factoryID));
    CFPlugInAddInstanceForFactory( factoryID );
  }
}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::~IOHIDPointerScrollFilter
//------------------------------------------------------------------------------
IOHIDPointerScrollFilter::~IOHIDPointerScrollFilter()
{
  if (_factoryID) {
    CFPlugInRemoveInstanceForFactory( _factoryID );
    CFRelease( _factoryID );
  }
  if (_pointerAccelerator) {
    delete _pointerAccelerator;
  }
  for (size_t index = 0; index < sizeof(_scrollAccelerators)/sizeof(_scrollAccelerators[0]); index++) {
    if (_scrollAccelerators[index]) {
      delete _scrollAccelerators[index];
    }
  }
    
}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::QueryInterface
//------------------------------------------------------------------------------
HRESULT IOHIDPointerScrollFilter::QueryInterface( void *self, REFIID iid, LPVOID *ppv )
{
    return static_cast<IOHIDPointerScrollFilter *>(self)->QueryInterface(iid, ppv);
}

// Implementation of the IUnknown QueryInterface function.
HRESULT IOHIDPointerScrollFilter::QueryInterface( REFIID iid, LPVOID *ppv )
{
    // Create a CoreFoundation UUIDRef for the requested interface.
    CFUUIDRef interfaceID = CFUUIDCreateFromUUIDBytes( NULL, iid );
    
    // Test the requested ID against the valid interfaces.
    if (CFEqual(interfaceID, kIOHIDSimpleServiceFilterPlugInInterfaceID) || CFEqual(interfaceID, kIOHIDServiceFilterPlugInInterfaceID)) {
        AddRef();
        *ppv = this;
        CFRelease(interfaceID);
        return S_OK;
    }
    if (CFEqual(interfaceID, IUnknownUUID)) {
        // If the IUnknown interface was requested, same as above.
        AddRef();
        *ppv = this;
        CFRelease(interfaceID);
        return S_OK;
    }
    // Requested interface unknown, bail with error.
    *ppv = NULL;
    CFRelease( interfaceID );
    return E_NOINTERFACE;
}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::AddRef
//------------------------------------------------------------------------------
ULONG IOHIDPointerScrollFilter::AddRef( void *self )
{
    return static_cast<IOHIDPointerScrollFilter *>(self)->AddRef();
}

ULONG IOHIDPointerScrollFilter::AddRef()
{
    _refCount += 1;
    return _refCount;
}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::Release
//------------------------------------------------------------------------------
ULONG IOHIDPointerScrollFilter::Release( void *self )
{
    return static_cast<IOHIDPointerScrollFilter *>(self)->Release();
}

ULONG IOHIDPointerScrollFilter::Release()
{
    _refCount -= 1;
    if (_refCount == 0) {
        delete this;
        return 0;
    }
    return _refCount;
}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::open
//------------------------------------------------------------------------------
void IOHIDPointerScrollFilter::open(void * self, IOHIDServiceRef service, IOOptionBits options)
{
    static_cast<IOHIDPointerScrollFilter *>(self)->open(service, options);
}

void IOHIDPointerScrollFilter::open(IOHIDServiceRef service, IOOptionBits options __unused)
{
  
    _service = service;

    CFTypeRef leagcyShim = IOHIDServiceCopyProperty(service, CFSTR(kIOHIDCompatibilityInterface));
    if (leagcyShim) {
      _leagacyShim = true;
      CFRelease(leagcyShim);
    }

}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::close
//------------------------------------------------------------------------------
void IOHIDPointerScrollFilter::close(void * self, IOHIDServiceRef service, IOOptionBits options)
{
    static_cast<IOHIDPointerScrollFilter *>(self)->close(service, options);
}

void IOHIDPointerScrollFilter::close(IOHIDServiceRef service __unused, IOOptionBits options __unused)
{
    _service = NULL;
}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::scheduleWithDispatchQueue
//------------------------------------------------------------------------------
void IOHIDPointerScrollFilter::scheduleWithDispatchQueue(void * self, dispatch_queue_t queue)
{
    static_cast<IOHIDPointerScrollFilter *>(self)->scheduleWithDispatchQueue(queue);
}

void IOHIDPointerScrollFilter::scheduleWithDispatchQueue(dispatch_queue_t queue)
{
    _queue = queue;
 
    setupAcceleration();
}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::unscheduleFromDispatchQueue
//------------------------------------------------------------------------------
void IOHIDPointerScrollFilter::unscheduleFromDispatchQueue(void * self, dispatch_queue_t queue)
{
    static_cast<IOHIDPointerScrollFilter *>(self)->unscheduleFromDispatchQueue(queue);
}

void IOHIDPointerScrollFilter::unscheduleFromDispatchQueue(dispatch_queue_t queue __unused)
{
    _queue = NULL;
    
}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::setEventCallback
//------------------------------------------------------------------------------
void IOHIDPointerScrollFilter::setEventCallback(void * self, IOHIDServiceEventCallback callback, void * target, void * refcon)
{
    static_cast<IOHIDPointerScrollFilter *>(self)->setEventCallback(callback, target, refcon);
}

void IOHIDPointerScrollFilter::setEventCallback(IOHIDServiceEventCallback callback, void * target, void * refcon)
{
    _eventCallback  = callback;
    _eventTarget    = target;
    _eventContext   = refcon;
}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::copyPropertyForClient
//------------------------------------------------------------------------------
CFTypeRef IOHIDPointerScrollFilter::copyPropertyForClient(void * self, CFStringRef key, CFTypeRef client)
{
    return static_cast<IOHIDPointerScrollFilter *>(self)->copyPropertyForClient(key, client);
}

CFTypeRef IOHIDPointerScrollFilter::copyPropertyForClient(CFStringRef key, CFTypeRef client __unused)
{

    CFTypeRef result = NULL;
  
    if (CFEqual(key, CFSTR(kIOHIDServiceFilterDebugKey))) {
        CFMutableDictionaryRefWrap serializer;
        serialize(serializer);
        if (serializer) {
          result = CFRetain(serializer.Reference());
        }
    }

    return result;
}

CFStringRef IOHIDPointerScrollFilter::_cachedPropertyList[] = {
    CFSTR(kIOHIDPointerAccelerationKey),
    CFSTR(kIOHIDScrollAccelerationKey),
    CFSTR(kIOHIDMouseAccelerationType),
    CFSTR(kIOHIDMouseScrollAccelerationKey),
    CFSTR(kIOHIDTrackpadScrollAccelerationKey),
    CFSTR(kIOHIDTrackpadAccelerationType),
    CFSTR(kIOHIDScrollAccelerationTypeKey),
    CFSTR(kIOHIDPointerAccelerationTypeKey),
    CFSTR(kIOHIDUserPointerAccelCurvesKey),
    CFSTR(kIOHIDUserScrollAccelCurvesKey),
    CFSTR(kIOHIDPointerAccelerationMultiplierKey),
    CFSTR(kIOHIDPointerAccelerationSupportKey),
    CFSTR(kIOHIDScrollAccelerationSupportKey),
    CFSTR(kHIDPointerReportRateKey),
    CFSTR(kIOHIDScrollReportRateKey),
};

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::setPropertyForClient
//------------------------------------------------------------------------------
void IOHIDPointerScrollFilter::setPropertyForClient(void * self,CFStringRef key,CFTypeRef property,CFTypeRef client)
{
    static_cast<IOHIDPointerScrollFilter *>(self)->setPropertyForClient(key, property, client);
}

void IOHIDPointerScrollFilter::setPropertyForClient(CFStringRef key,CFTypeRef property,CFTypeRef client __unused)
{
  bool  updated = false;
  if (key == NULL) {
      return;
  }
  
  for (size_t index = 0 ; index < sizeof(_cachedPropertyList) / sizeof(_cachedPropertyList[0]); index++) {
      if (CFEqual(key,  _cachedPropertyList[index])) {
          _cachedProperty.SetValueForKey(key, property);
          _property.SetValueForKey(key, property);
          updated = true;
          break;
      }
  }
  if (updated) {
      HIDLog("[%@] Acceleration key:%@ value:%@ apply:%s client:%@", SERVICE_ID, key, property, _queue ? "yes" : "no", client);
  }
  if (updated && _queue) {
      setupAcceleration ();
      _cachedProperty.RemoveAll();
  }
  return;
}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::match
//------------------------------------------------------------------------------
SInt32 IOHIDPointerScrollFilter::match(void * self, IOHIDServiceRef service, IOOptionBits options)
{
    return static_cast<IOHIDPointerScrollFilter *>(self)->match(service, options);
}

SInt32 IOHIDPointerScrollFilter::match(IOHIDServiceRef service, IOOptionBits options __unused)
{
    _matchScore = (IOHIDServiceConformsTo(service, kHIDPage_GenericDesktop, kHIDUsage_GD_Mouse) ||
                   IOHIDServiceConformsTo(service, kHIDPage_GenericDesktop, kHIDUsage_GD_Pointer) ||
                   IOHIDServiceConformsTo(service, kHIDPage_AppleVendor,    kHIDUsage_GD_Pointer) ||
                   IOHIDServiceConformsTo(service, kHIDPage_Digitizer,    kHIDUsage_Dig_TouchPad)) ? 100 : 0;
    
    HIDLogDebug("(%p) for ServiceID %@ with score %d", this, IOHIDServiceGetRegistryID(service), (int)_matchScore);
    
    return _matchScore;
}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::filter
//------------------------------------------------------------------------------
IOHIDEventRef IOHIDPointerScrollFilter::filter(void * self, IOHIDEventRef event)
{
  return static_cast<IOHIDPointerScrollFilter *>(self)->filter(event);
}

IOHIDEventRef IOHIDPointerScrollFilter::filter(IOHIDEventRef event)
{
  if (!event) {
      return event;
  }

  if ((_pointerAccelerator && IOHIDEventConformsTo (event, kIOHIDEventTypePointer) && !IOHIDEventIsAbsolute(event)) ||
     ((_scrollAccelerators[0] || _scrollAccelerators[1] || _scrollAccelerators[2]) && IOHIDEventConformsTo (event, kIOHIDEventTypeScroll))) {
    accelerateEvent (event);
  }
  return event;
}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::accelerateChildrens
//------------------------------------------------------------------------------
void IOHIDPointerScrollFilter::accelerateChildrens(IOHIDEventRef event) {
  
  CFArrayRef children = IOHIDEventGetChildren (event);
  for (CFIndex index = 0 , count = children ? CFArrayGetCount(children) : 0 ; index < count ; index++) {
    accelerateEvent ((IOHIDEventRef)CFArrayGetValueAtIndex(children, index));
  }

}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::accelerateEvent
//------------------------------------------------------------------------------
void IOHIDPointerScrollFilter::accelerateEvent(IOHIDEventRef event) {
  bool accelerated;
  IOHIDEventRef accelEvent;

  if (_pointerAccelerator &&
      _pointerAccelerationSupported &&
      IOHIDEventGetType(event) == kIOHIDEventTypePointer &&
      !(IOHIDEventGetEventFlags(event) & kIOHIDPointerEventOptionsNoAcceleration)) {
    double xy[2];
    if ((IOHIDEventGetEventFlags(event) & kIOHIDAccelerated) == 0) {
      
      xy[0] = IOHIDEventGetFloatValue (event, kIOHIDEventFieldPointerX);
      xy[1] = IOHIDEventGetFloatValue (event, kIOHIDEventFieldPointerY);
      if (xy[0] || xy[1]) {
        accelerated = _pointerAccelerator->accelerate(xy, sizeof (xy) / sizeof(xy[0]), IOHIDEventGetTimeStamp(event));
        
        if (accelerated && (accelEvent = IOHIDEventCreateCopy(kCFAllocatorDefault, event)) != NULL) {
          CFMutableArrayRef children = (CFMutableArrayRef)IOHIDEventGetChildren(accelEvent);
          if (children) {
            CFArrayRemoveAllValues(children);
          }
          IOHIDEventSetFloatValue (accelEvent, kIOHIDEventFieldPointerX, xy[0]);
          IOHIDEventSetFloatValue (accelEvent, kIOHIDEventFieldPointerY, xy[1]);
          IOHIDEventSetEventFlags (accelEvent, IOHIDEventGetEventFlags(accelEvent) | kIOHIDAccelerated);
          IOHIDEventAppendEvent (event, accelEvent, 0);
          CFRelease(accelEvent);
        }
      }
    }
  }
  if (_scrollAccelerationSupported &&
      IOHIDEventGetType(event) == kIOHIDEventTypeScroll &&
      !(IOHIDEventGetEventFlags(event) & kIOHIDScrollEventOptionsNoAcceleration)) {
    if ((IOHIDEventGetEventFlags(event) & kIOHIDAccelerated) == 0) {
      static int axis [3] = {kIOHIDEventFieldScrollX, kIOHIDEventFieldScrollY, kIOHIDEventFieldScrollZ};
      double value [3];
      accelerated = false;

      if (IOHIDEventGetScrollMomentum(event) != 0) {
        CFTypeRef momentumScrollRate = _IOHIDEventCopyAttachment(event, CFSTR("ScrollMomentumDispatchRate"), 0);

        if (momentumScrollRate && CFGetTypeID(momentumScrollRate) == CFNumberGetTypeID()) {
          CFNumberRef momentumScrollValue = (CFNumberRef)momentumScrollRate;
          double prevScrollMomentumMult = _scrollMomentumMult;
          float dispatchRate = kIOHIDDefaultReportRate;
          
          CFNumberGetValue(momentumScrollValue, kCFNumberFloatType, &dispatchRate);
          _scrollMomentumMult = dispatchRate / kIOHIDDefaultReportRate;

          if (fabs(prevScrollMomentumMult  - _scrollMomentumMult) > 0.5) {
            HIDLogInfo("[%@] _scrollMomentumMult:%.3f->%.3f", SERVICE_ID, prevScrollMomentumMult, _scrollMomentumMult);
          }
        } else {
          _scrollMomentumMult = 1.0;
        }


        if (momentumScrollRate) {
          CFRelease(momentumScrollRate);
          momentumScrollRate = NULL;
        }
      } else {
        _scrollMomentumMult = 1.0;
      }

      
      for (int  index = 0; index < (int)(sizeof(axis) / sizeof(axis[0])); index++) {
        value[index] = IOHIDEventGetFloatValue (event, axis[index]);
        if (value[index] != 0 && _scrollAccelerators[index] != NULL) {
          double *scrollValue = (value + index);
          *scrollValue *= _scrollMomentumMult;
          accelerated |=_scrollAccelerators[index]->accelerate(scrollValue, 1, IOHIDEventGetTimeStamp(event));
          *scrollValue /= _scrollMomentumMult;
        }
      }
      if (accelerated && (accelEvent = IOHIDEventCreateCopy(kCFAllocatorDefault, event)) != NULL) {
        CFMutableArrayRef children = (CFMutableArrayRef) IOHIDEventGetChildren(accelEvent);
        if (children) {
          CFArrayRemoveAllValues(children);
        }
        for (int  index = 0; index < (int)(sizeof(axis) / sizeof(axis[0])); index++) {
          IOHIDEventSetFloatValue (accelEvent, axis[index], value[index]);
        }
        IOHIDEventSetEventFlags (accelEvent, IOHIDEventGetEventFlags(accelEvent) | kIOHIDAccelerated);
        IOHIDEventAppendEvent (event, accelEvent, 0);
        CFRelease(accelEvent);
      }
    }
  }
  accelerateChildrens(event);
}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::setupPointerAcceleration
//------------------------------------------------------------------------------
void IOHIDPointerScrollFilter::setupPointerAcceleration(double pointerAccelerationMultiplier)
{
  
  if (_leagacyShim) {
    if (_pointerAccelerator == NULL) {
      _pointerAccelerator = new IOHIDSimpleAccelerator(LEGACY_SHIM_POINTER_DELTA_MULTIPLIER);
    }
    return;
  }
  
  IOHIDAccelerator *tmp = _pointerAccelerator;
  _pointerAccelerator = NULL;

  if (tmp) {
    delete tmp;
  }


  CFBooleanRefWrap enabled = CFBooleanRefWrap((CFBooleanRef)copyCachedProperty(CFSTR(kIOHIDPointerAccelerationSupportKey)), true);
  if (enabled.Reference() == NULL || (bool)enabled) {
    _pointerAccelerationSupported = true;
  } else {
    _pointerAccelerationSupported = false;
  }
  
  CFNumberRefWrap resolution = CFNumberRefWrap((CFNumberRef)IOHIDServiceCopyProperty (_service, CFSTR(kIOHIDPointerResolutionKey)), true);
  
  if (resolution.Reference() == NULL || (SInt32)resolution == 0) {
    resolution = CFNumberRefWrap((SInt32) kDefaultPointerResolutionFixed);
    if (resolution.Reference () == NULL) {
      HIDLogInfo("[%@] Could not get/create pointer resolution", SERVICE_ID);
      return;
    }
  }
  
    CFNumberRefWrap defaultRate = CFNumberRefWrap((CFNumberRef)copyCachedProperty(CFSTR(kHIDPointerReportRateKey)), true);
    if (defaultRate.Reference() == NULL) {
        defaultRate = CFNumberRefWrap((SInt32) 0);
        if (defaultRate.Reference () == NULL) {
            HIDLogInfo ("[%@] Could not get/create pointer report rate", SERVICE_ID);
            return;
        }
    }
    
    CFRefWrap<CFTypeRef>  pointerAccelType(NULL);
    CFRefWrap<CFTypeRef>  typePointerAccel(NULL);
    CFRefWrap<CFTypeRef>  mousePointerAccel(NULL);
    CFRefWrap<CFTypeRef>  basicPointerAccel(NULL);

    CFNumberRefWrap pointerAcceleration;
  
    pointerAccelType = CFRefWrap<CFTypeRef>(copyCachedProperty (CFSTR(kIOHIDPointerAccelerationTypeKey)), true);

    if (pointerAccelType.Reference()) {
        typePointerAccel =  CFRefWrap<CFTypeRef> (copyCachedProperty((CFStringRef)pointerAccelType.Reference()), true);
        pointerAcceleration = CFNumberRefWrap((CFNumberRef)typePointerAccel.Reference());
    }
    if (pointerAcceleration.Reference() == NULL) {
        mousePointerAccel = CFRefWrap<CFTypeRef> (copyCachedProperty(CFSTR(kIOHIDMouseAccelerationType)), true);
        pointerAcceleration = CFNumberRefWrap((CFNumberRef)mousePointerAccel.Reference());
    }
    if (pointerAcceleration.Reference() == NULL) {
        basicPointerAccel =  CFRefWrap<CFTypeRef> (copyCachedProperty(CFSTR(kIOHIDPointerAccelerationKey)), true);
        pointerAcceleration = CFNumberRefWrap((CFNumberRef)basicPointerAccel.Reference());
    }
    if (pointerAcceleration.Reference()) {
        _pointerAcceleration = FIXED_TO_DOUBLE((SInt32)pointerAcceleration);
    }
    HIDLog ("[%@] Pointer acceleration (%s) %@:%@ %s:%@ %s:%@ %@",
            SERVICE_ID, _pointerAcceleration < 0 ? "disabled" : "enabled",
            pointerAccelType.Reference(), typePointerAccel.Reference(),
            kIOHIDMouseAccelerationType, mousePointerAccel.Reference(),
            kIOHIDPointerAccelerationKey, basicPointerAccel.Reference(),
            pointerAcceleration.Reference());

    if (_pointerAcceleration < 0) {
        return;
    }

  HIDLogDebug("[%@] Pointer acceleration value %f", SERVICE_ID, _pointerAcceleration);

  IOHIDAccelerationAlgorithm * algorithm = NULL;
  
  CFArrayRefWrap userCurves ((CFArrayRef)IOHIDServiceCopyProperty (_service, CFSTR(kIOHIDUserPointerAccelCurvesKey)), true);
  if (userCurves && userCurves.Count () > 0) {
    algorithm = IOHIDParametricAcceleration::CreateWithParameters(
                                                 userCurves,
                                                 _pointerAcceleration,
                                                 FIXED_TO_DOUBLE((SInt32)resolution),
                                                 FRAME_RATE
                                                 );
  } else {
  
    CFRefWrap<CFArrayRef> driverCurves ((CFArrayRef)IOHIDServiceCopyProperty (_service, CFSTR(kHIDAccelParametricCurvesKey)), true);
    if (driverCurves.Reference()) {
      algorithm = IOHIDParametricAcceleration::CreateWithParameters(
                                                 driverCurves,
                                                 _pointerAcceleration,
                                                 FIXED_TO_DOUBLE((SInt32)resolution),
                                                 FRAME_RATE
                                                 );
    } else {
      CFRefWrap<CFDataRef> table ((CFDataRef)IOHIDServiceCopyProperty (_service, CFSTR(kIOHIDPointerAccelerationTableKey)), true);
  
      if (table.Reference() == NULL) {
        table =  CFRefWrap<CFDataRef>(CFDataCreate(kCFAllocatorDefault, defaultAccelTable, sizeof (defaultAccelTable)), true);
      }
        
      if (table.Reference()) {
        algorithm = IOHIDTableAcceleration::CreateWithTable(
                                                   table,
                                                   _pointerAcceleration,
                                                   FIXED_TO_DOUBLE((SInt32)resolution),
                                                   FRAME_RATE
                                                   );
      }
    }
  }
  if (algorithm) {
    _pointerAccelerator = new IOHIDPointerAccelerator (algorithm, FIXED_TO_DOUBLE((SInt32)resolution), (SInt32)defaultRate, pointerAccelerationMultiplier);
  } else {
    HIDLogInfo("[%@] Could not create accelerator", SERVICE_ID);
  }
}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::setupScrollAcceleration
//------------------------------------------------------------------------------
void IOHIDPointerScrollFilter::setupScrollAcceleration(double scrollAccelerationMultiplier) {

  static CFStringRef ResolutionKeys[] = {
    CFSTR(kIOHIDScrollResolutionXKey),
    CFSTR(kIOHIDScrollResolutionYKey),
    CFSTR(kIOHIDScrollResolutionZKey)
  };
  static CFStringRef AccelTableKeys[] = {
    CFSTR(kIOHIDScrollAccelerationTableXKey),
    CFSTR(kIOHIDScrollAccelerationTableYKey),
    CFSTR(kIOHIDScrollAccelerationTableZKey)
  };

  if (_leagacyShim) {
    for (int  index = 0; index < (int)(sizeof(AccelTableKeys)/sizeof(AccelTableKeys[0])); index++) {
      if (_scrollAccelerators[index] == NULL) {
        _scrollAccelerators[index] = new IOHIDSimpleAccelerator(LEGACY_SHIM_SCROLL_DELTA_MULTIPLIER);
      }
    }
    return;
  }

  CFBooleanRefWrap enabled = CFBooleanRefWrap((CFBooleanRef)copyCachedProperty(CFSTR(kIOHIDScrollAccelerationSupportKey)), true);
  if (enabled.Reference() == NULL || (bool)enabled) {
    _scrollAccelerationSupported = true;
  } else {
    _scrollAccelerationSupported = false;
  }
  
    CFNumberRefWrap       scrollAcceleration;

    CFRefWrap<CFTypeRef>  scrollAccelType(NULL);
    CFRefWrap<CFTypeRef>  typeScrollAccel(NULL);
    CFRefWrap<CFTypeRef>  mouseScrollAccel(NULL);
    CFRefWrap<CFTypeRef>  basicScrollAccel(NULL);

    scrollAccelType = CFRefWrap<CFTypeRef>(copyCachedProperty (CFSTR(kIOHIDScrollAccelerationTypeKey)), true);
  
    if (scrollAccelType.Reference()) {
        typeScrollAccel = CFRefWrap<CFTypeRef>(copyCachedProperty((CFStringRef)scrollAccelType.Reference()), true);
        scrollAcceleration = CFNumberRefWrap((CFNumberRef)typeScrollAccel.Reference());
    }
    if (scrollAcceleration.Reference() == NULL) {
        mouseScrollAccel = CFRefWrap<CFTypeRef>(copyCachedProperty(CFSTR(kIOHIDMouseScrollAccelerationKey)), true);
        scrollAcceleration = CFNumberRefWrap((CFNumberRef)mouseScrollAccel.Reference());
    }
    if (scrollAcceleration.Reference() == NULL) {
        basicScrollAccel = CFRefWrap<CFTypeRef>(copyCachedProperty(CFSTR(kIOHIDScrollAccelerationKey)), true);
        scrollAcceleration = CFNumberRefWrap((CFNumberRef)basicScrollAccel.Reference());
    }
    if (scrollAcceleration.Reference()) {
        _scrollAcceleration = FIXED_TO_DOUBLE((SInt32)scrollAcceleration);
    }

    HIDLog("[%@] Scroll acceleration (%s) %@:%@ %s:%@ %s:%@ %@",
            SERVICE_ID, _scrollAcceleration < 0 ? "disabled" : "enabled",
            scrollAccelType.Reference(), typeScrollAccel.Reference(),
            kIOHIDMouseScrollAccelerationKey, mouseScrollAccel.Reference(),
            kIOHIDScrollAccelerationKey, basicScrollAccel.Reference(),
            scrollAcceleration.Reference()
            );

    if (_scrollAcceleration < 0) {
        return;
    }
  
  HIDLogDebug("[%@] Scroll acceleration value %f", SERVICE_ID, _scrollAcceleration);
  
  CFNumberRefWrap rate = CFNumberRefWrap((CFNumberRef)copyCachedProperty(CFSTR(kIOHIDScrollReportRateKey)), true);
  if (rate.Reference() == NULL || (SInt32)rate == 0) {
    rate = CFNumberRefWrap((SInt32)((int)FRAME_RATE << 16));
    if (rate.Reference() == NULL) {
      HIDLogInfo("[%@] Could not get/create report rate", SERVICE_ID);
      return;
    }
  }
  
  for (int  index = 0; index < (int)(sizeof(AccelTableKeys)/sizeof(AccelTableKeys[0])); index++) {
    
    IOHIDAccelerationAlgorithm * algorithm = NULL;
    
    IOHIDAccelerator *tmp = _scrollAccelerators[index];
    _scrollAccelerators[index] = NULL;

    if (tmp) {
      delete tmp;
    }
    
    CFNumberRefWrap resolution = CFNumberRefWrap((CFNumberRef)IOHIDServiceCopyProperty (_service, ResolutionKeys[index]), true);
    if (resolution.Reference() == NULL) {
      resolution = CFNumberRefWrap((CFNumberRef)IOHIDServiceCopyProperty (_service, CFSTR(kIOHIDScrollResolutionKey)), true);
    }
    if (resolution.Reference() == NULL) {
      HIDLogInfo("[%@] Could not get kIOHIDScrollResolutionKey", SERVICE_ID);
      continue;
    }

    CFArrayRefWrap userCurves ((CFArrayRef)IOHIDServiceCopyProperty (_service, CFSTR(kIOHIDUserScrollAccelCurvesKey)), true);
    if (userCurves &&  userCurves.Count() > 0) {
        algorithm = IOHIDParametricAcceleration::CreateWithParameters(
                                                  userCurves,
                                                  _scrollAcceleration,
                                                  FIXED_TO_DOUBLE((SInt32)resolution),
                                                  FIXED_TO_DOUBLE((SInt32)rate)
                                                  );
    } else {
      CFRefWrap<CFArrayRef> driverCurves ((CFArrayRef)IOHIDServiceCopyProperty (_service, CFSTR(kHIDScrollAccelParametricCurvesKey)), true);
      if (driverCurves.Reference()) {
        algorithm = IOHIDParametricAcceleration::CreateWithParameters(
                                                   driverCurves,
                                                   _scrollAcceleration,
                                                   FIXED_TO_DOUBLE((SInt32)resolution),
                                                   FIXED_TO_DOUBLE((SInt32)rate)
                                                   );
      } else {
        
        CFRefWrap<CFDataRef> table ((CFDataRef)IOHIDServiceCopyProperty(_service, AccelTableKeys[index]), true);
        
        if (table.Reference() == NULL) {
          table = CFRefWrap<CFDataRef>((CFDataRef)IOHIDServiceCopyProperty(_service, CFSTR(kIOHIDScrollAccelerationTableKey)), true);
        }
        
        if (table.Reference() == NULL) {
            table =  CFRefWrap<CFDataRef>(CFDataCreate(kCFAllocatorDefault, defaultAccelTable, sizeof (defaultAccelTable)), true);
        }
      
        if (table.Reference()) {
           algorithm = IOHIDTableAcceleration::CreateWithTable (
                                                     table,
                                                     _scrollAcceleration,
                                                     FIXED_TO_DOUBLE((SInt32)resolution),
                                                     FIXED_TO_DOUBLE((SInt32)rate)
                                                     );
        }
      }
    }
    if (algorithm) {
      _scrollAccelerators[index] = new IOHIDScrollAccelerator(algorithm, FIXED_TO_DOUBLE((SInt32)resolution), FIXED_TO_DOUBLE((SInt32)rate), scrollAccelerationMultiplier);
    }
  }
}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::setAcceleration
//------------------------------------------------------------------------------
void IOHIDPointerScrollFilter::setupAcceleration()
{
  if (!_service) {
    HIDLogDebug("(%p) setupAcceleration service not available", this);
    return;
  }

  CFNumberRefWrap pointerAccelerationMultiplier = CFNumberRefWrap((CFNumberRef)copyCachedProperty(CFSTR(kIOHIDPointerAccelerationMultiplierKey)), true);
  if (pointerAccelerationMultiplier.Reference() == NULL || (SInt32)pointerAccelerationMultiplier == 0) {
     pointerAccelerationMultiplier = CFNumberRefWrap((SInt32) DOUBLE_TO_FIXED((double)(1.0)));
     if (pointerAccelerationMultiplier.Reference() == NULL) {
       HIDLogInfo("[%@] Could not get/create pointer acceleration multiplier", SERVICE_ID);
       return;
     }
  }
  setupPointerAcceleration(FIXED_TO_DOUBLE((SInt32)pointerAccelerationMultiplier));

  // @reado scroll acceleration logic without timestamp
  // for now no need for fixed multiplier since timestamp
  // is average over multiple packets , so it's not same problem
  // as pointer accleration where rate multiplier considers
  // delta between two packets
  setupScrollAcceleration (1.0);
}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::getCachedProperty
//------------------------------------------------------------------------------
CFTypeRef IOHIDPointerScrollFilter::copyCachedProperty (CFStringRef key)  const {
    CFTypeRef value = _cachedProperty [key];
    if (value) {
        CFRetain(value);
        return value;
    }
    value = IOHIDServiceCopyProperty (_service, key);
    if (value) {
        return value;
    }
    value = _property[key];
    if (value) {
        CFRetain(value);
    }
    return value;
}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::serialize
//------------------------------------------------------------------------------
void IOHIDPointerScrollFilter::serialize (CFMutableDictionaryRef dict) const {
  CFMutableDictionaryRefWrap serializer (dict);
  const char * axis[] = {"X", "Y", "Z"};
  if (serializer.Reference() == NULL) {
      return;
  }
  
  CFRefWrap<CFStringRef>  pointerAccelerationType ((CFStringRef)copyCachedProperty (CFSTR(kIOHIDPointerAccelerationTypeKey)), true);
  if (pointerAccelerationType) {
      serializer.SetValueForKey(CFSTR(kIOHIDPointerAccelerationTypeKey), pointerAccelerationType.Reference());
  }

  CFRefWrap<CFStringRef>  scrollAccelerationType ((CFStringRef)copyCachedProperty (CFSTR(kIOHIDScrollAccelerationTypeKey)), true);
  if (scrollAccelerationType) {
      serializer.SetValueForKey(CFSTR(kIOHIDScrollAccelerationTypeKey), scrollAccelerationType.Reference());
  }
  
  serializer.SetValueForKey(CFSTR("Class"), CFSTR("IOHIDPointerScrollFilter"));
  serializer.SetValueForKey(CFSTR("PointerAccelerationValue"), DOUBLE_TO_FIXED(_pointerAcceleration));
  serializer.SetValueForKey(CFSTR("ScrollAccelerationValue") , DOUBLE_TO_FIXED(_scrollAcceleration));
  serializer.SetValueForKey(CFSTR("MatchScore"), (uint64_t)_matchScore);
  serializer.SetValueForKey(CFSTR("Property"), _property.Reference());
   
  if (_pointerAccelerator) {
      CFMutableDictionaryRefWrap pa;
      _pointerAccelerator->serialize (pa);
      serializer.SetValueForKey(CFSTR("Pointer Accelerator"), pa);
  }
  for (size_t i = 0 ; i < sizeof(_scrollAccelerators)/ sizeof(_scrollAccelerators[0]); i++) {
      if (_scrollAccelerators[i]) {
          CFMutableDictionaryRefWrap sa;
          CFStringRefWrap axiskey (std::string("Scroll Accelerator ( axis: ") + std::string(axis[i]) +  std::string(")"));
          _scrollAccelerators[i]->serialize(sa);
          serializer.SetValueForKey(axiskey, sa);
      }
  }
}

//
//  HIDEventAccessors_Private.h
//

//
// DO NOT EDIT THIS FILE. IT IS AUTO-GENERATED
//

#ifndef HIDEventAccessors_Private_h
#define HIDEventAccessors_Private_h

#import <Foundation/Foundation.h>
#import <HID/HIDEvent.h>
#import <HID/HIDEventAccessors.h>

typedef uint32_t IOHIDGenericGestureType;
typedef uint8_t IOHIDEventColorSpace;

NS_ASSUME_NONNULL_BEGIN

@interface HIDEvent (HIDVendorDefinedEventPrivate)

@property (readonly) uint8_t * vendorDefinedData;
@property (readonly) uint32_t vendorDefinedDataLength;
@property uint16_t vendorDefinedUsage;
@property uint16_t vendorDefinedUsagePage;
@property uint32_t vendorDefinedVersion;
@end

@interface HIDEvent (HIDTemperatureEventPrivate)

@property double temperatureLevel;
@end

@interface HIDEvent (HIDAccelerometerEventPrivate)

@property uint32_t accelerometerSequence;
@property uint32_t accelerometerSubType;
@property uint32_t accelerometerType;
@property double accelerometerX;
@property double accelerometerY;
@property double accelerometerZ;
@end

@interface HIDEvent (HIDGenericGestureEventPrivate)

@property (readonly) IOHIDGenericGestureType genericGestureType;
@property double genericGestureTypeSwipeProgress;
@property uint32_t genericGestureTypeTapCount;
@end

@interface HIDEvent (HIDAmbientLightSensorEventPrivate)

@property double ambientLightColorComponent0;
@property double ambientLightColorComponent1;
@property double ambientLightColorComponent2;
@property IOHIDEventColorSpace ambientLightColorSpace;
@property bool ambientLightDisplayBrightnessChanged;
@property double ambientLightSensorColorTemperature;
@property double ambientLightSensorIlluminance;
@property uint32_t ambientLightSensorLevel;
@property uint32_t ambientLightSensorRawChannel0;
@property uint32_t ambientLightSensorRawChannel1;
@property uint32_t ambientLightSensorRawChannel2;
@property uint32_t ambientLightSensorRawChannel3;
@end

@interface HIDEvent (HIDPowerEventPrivate)

@property double powerMeasurement;
@property uint32_t powerSubType;
@property uint32_t powerType;
@end

@interface HIDEvent (HIDForceEventPrivate)

@property uint32_t forceBehavior;
@property double forceProgress;
@property uint32_t forceStage;
@property double forceStagePressure;
@end

@interface HIDEvent (HIDMotionGestureEventPrivate)

@property uint32_t motionGestureGestureType;
@property double motionGestureProgress;
@end

@interface HIDEvent (HIDGameControllerEventPrivate)

@property double gameControllerButtonL4;
@property double gameControllerButtonL5;
@property double gameControllerButtonR4;
@property double gameControllerButtonR5;
@property double gameControllerDirectionPadDown;
@property double gameControllerDirectionPadLeft;
@property double gameControllerDirectionPadRight;
@property double gameControllerDirectionPadUp;
@property double gameControllerFaceButtonA;
@property double gameControllerFaceButtonB;
@property double gameControllerFaceButtonX;
@property double gameControllerFaceButtonY;
@property double gameControllerJoyStickAxisRz;
@property double gameControllerJoyStickAxisX;
@property double gameControllerJoyStickAxisY;
@property double gameControllerJoyStickAxisZ;
@property double gameControllerShoulderButtonL1;
@property double gameControllerShoulderButtonL2;
@property double gameControllerShoulderButtonR1;
@property double gameControllerShoulderButtonR2;
@property uint32_t gameControllerThumbstickButtonLeft;
@property uint32_t gameControllerThumbstickButtonRight;
@property uint32_t gameControllerType;
@end

@interface HIDEvent (HIDDigitizerEventPrivate)

@property double digitizerAltitude;
@property double digitizerAuxiliaryPressure;
@property double digitizerAzimuth;
@property uint32_t digitizerButtonMask;
@property uint32_t digitizerChildEventMask;
@property uint32_t digitizerCollection;
@property double digitizerDensity;
@property uint32_t digitizerDidUpdateMask;
@property uint32_t digitizerEventMask;
@property uint32_t digitizerGenerationCount;
@property uint32_t digitizerIdentity;
@property uint32_t digitizerIndex;
@property double digitizerIrregularity;
@property uint32_t digitizerIsDisplayIntegrated;
@property double digitizerMajorRadius;
@property double digitizerMinorRadius;
@property (readonly) uint32_t digitizerOrientationType;
@property double digitizerPressure;
@property double digitizerQuality;
@property double digitizerQualityRadiiAccuracy;
@property uint32_t digitizerRange;
@property double digitizerTiltX;
@property double digitizerTiltY;
@property uint32_t digitizerTouch;
@property double digitizerTwist;
@property uint32_t digitizerType;
@property uint32_t digitizerWillUpdateMask;
@property double digitizerX;
@property double digitizerY;
@property double digitizerZ;
@end

@interface HIDEvent (HIDCompassEventPrivate)

@property uint32_t compassSequence;
@property uint32_t compassSubType;
@property uint32_t compassType;
@property double compassX;
@property double compassY;
@property double compassZ;
@end

@interface HIDEvent (HIDMotionActivityEventPrivate)

@property uint32_t motionActivityActivityType;
@property double motionActivityConfidence;
@end

@interface HIDEvent (HIDBrightnessEventPrivate)

@property double currentBrightness;
@property double targetBrightness;
@property uint64_t transitionTime;
@end

@interface HIDEvent (HIDGyroEventPrivate)

@property uint32_t gyroSequence;
@property uint32_t gyroSubType;
@property uint32_t gyroType;
@property double gyroX;
@property double gyroY;
@property double gyroZ;
@end

@interface HIDEvent (HIDButtonEventPrivate)

@property uint8_t buttonClickCount;
@property uint32_t buttonMask;
@property uint8_t buttonNumber;
@property double buttonPressure;
@property uint32_t buttonState;
@end

@interface HIDEvent (HIDAtmosphericPressureEventPrivate)

@property double atmosphericPressureLevel;
@property uint32_t atmosphericSequence;
@end

@interface HIDEvent (HIDHumidityEventPrivate)

@property double humidityRH;
@property uint32_t humiditySequence;
@end

@interface HIDEvent (HIDScrollEventPrivate)

@property uint32_t scrollIsPixels;
@property double scrollX;
@property double scrollY;
@property double scrollZ;
@end

@interface HIDEvent (HIDBiometricEventPrivate)

@property uint32_t biometricEventType;
@property double biometricLevel;
@property uint8_t biometricTapCount;
@property uint16_t biometricUsage;
@property uint16_t biometricUsagePage;
@end

@interface HIDEvent (HIDLEDEventPrivate)

@property uint32_t ledMask;
@property uint8_t ledNumber;
@property uint32_t ledState;
@end

@interface HIDEvent (HIDOrientationEventPrivate)

@property double orientationAltitude;
@property double orientationAzimuth;
@property uint32_t orientationDeviceOrientationUsage;
@property (readonly) uint32_t orientationOrientationType;
@property double orientationQuatW;
@property double orientationQuatX;
@property double orientationQuatY;
@property double orientationQuatZ;
@property double orientationRadius;
@property double orientationTiltX;
@property double orientationTiltY;
@property double orientationTiltZ;
@end

@interface HIDEvent (HIDProximityEventPrivate)

@property uint32_t probabilityLevel;
@property uint16_t proximityDetectionMask;
@property uint32_t proximityLevel;
@property uint16_t proximityProximityType;
@end

@interface HIDEvent (HIDKeyboardEventPrivate)

@property uint32_t keyboardClickSpeed;
@property uint32_t keyboardDown;
@property uint32_t keyboardLongPress;
@property uint32_t keyboardMouseKeyToggle;
@property uint8_t keyboardPressCount;
@property uint32_t keyboardRepeat;
@property uint32_t keyboardSlowKeyPhase;
@property uint32_t keyboardStickyKeyPhase;
@property uint32_t keyboardStickyKeyToggle;
@property uint16_t keyboardUsage;
@property uint16_t keyboardUsagePage;
@end

@interface HIDEvent (HIDPointerEventPrivate)

@property uint32_t pointerButtonMask;
@property double pointerX;
@property double pointerY;
@property double pointerZ;
@end

NS_ASSUME_NONNULL_END

#endif /* HIDEventAccessors_Private_h */


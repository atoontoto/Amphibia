
#include <TargetConditionals.h>
#if TARGET_OS_IOS == 1
  #import <UIKit/UIKit.h>
#else
  #import <Cocoa/Cocoa.h>
#endif

#define IPLUG_AUVIEWCONTROLLER IPlugAUViewController_vAmphibia
#define IPLUG_AUAUDIOUNIT IPlugAUAudioUnit_vAmphibia
#import <AmphibiaAU/IPlugAUAudioUnit.h>
#import <AmphibiaAU/IPlugAUViewController.h>

//! Project version number for AmphibiaAU.
FOUNDATION_EXPORT double AmphibiaAUVersionNumber;

//! Project version string for AmphibiaAU.
FOUNDATION_EXPORT const unsigned char AmphibiaAUVersionString[];

@class IPlugAUViewController_vAmphibia;

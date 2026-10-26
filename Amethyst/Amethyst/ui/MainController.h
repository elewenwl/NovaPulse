#ifndef MainController_h
#define MainController_h

#import <UIKit/UIKit.h>
#import "CreditsController.h"
#import "Utilities.h"

#define initCardBtnOption(option, name, sel) (self.option = [[CardButton alloc] init:name _id:self _sel:@selector(sel)]).optionView
#define initBtnOption(option, name, detail, sel) (self.option = [[PopupOption alloc] initButtonOption:name _id:self _sel:@selector(sel) text:detail]).optionView
#define initImgBtnOption(option, name, sel) (self.option = [[PopupOption alloc] initImageButtonOption:name _id:self _sel:@selector(sel)]).optionView
#define initFullBtnOption(option, name, sel) (self.option = [[PopupOption alloc] initFullButtonOption:name _id:self _sel:@selector(sel)]).optionView
#define initTextFieldOption(option, name, _sel) (self.option = [UITextField createTextField:self sel:@selector(_sel) text:name])
#define initSwitchOption(option, name) (self.option = [[PopupOption alloc] initSwitchOption:name]).optionView
#define initOtherOption(option, func) (self.option = func)
#define initSubview(subview, func) [self.view addSubview:(self.subview = func)]
#define simpleTransform(w, h) (CGAffineTransformScale(CGAffineTransformMakeTranslation(0, 0), w, h))

#define finalizePopupCard(card) \
    [self.card setOverlays:self.blurOverlay gesture:self.gestureOverlay]; \
    self.card.cardView.transform = CGAffineTransformScale(CGAffineTransformMakeTranslation(0, 0), 0.9, 0.9); \
    [self.view addSubview:self.card.cardView];

#define setAnchorConstraint(target, src, offset) \
    [self.target constraintEqualToAnchor:self.src constant:offset]
    
#define setConstantConstraint(target, offset) \
    [self.target constraintEqualToConstant:offset]

#define centerButtonConstraint(target, ref) \
    [self.target.centerXAnchor constraintEqualToAnchor:self.ref.optionButton.centerXAnchor], \
    [self.target.centerYAnchor constraintEqualToAnchor:self.ref.optionButton.centerYAnchor]

#define settingsOptionConstraint(option, last) \
    [self.option.optionView.centerXAnchor constraintEqualToAnchor:self.settingsPopupCard.cardView.centerXAnchor], \
    [self.option.optionView.widthAnchor constraintEqualToConstant:amethyst.popupCardWidth-60], \
    [self.option.optionView.heightAnchor constraintEqualToConstant:([self isJailbroken] ? 35 : amethyst.popupCardHeight)], \
    [self.option.optionView.topAnchor constraintEqualToAnchor:self.last.topAnchor constant:60]

#define advancedOptionConstraint(option, last) \
    [self.option.optionView.centerXAnchor constraintEqualToAnchor:self.advancedPopupCard.cardView.centerXAnchor], \
    [self.option.optionView.widthAnchor constraintEqualToConstant:amethyst.popupCardWidth-60], \
    [self.option.optionView.heightAnchor constraintEqualToConstant:amethyst.popupCardHeight], \
    [self.option.optionView.topAnchor constraintEqualToAnchor:self.last.topAnchor constant:60]

#define popupCardConstraint(card, width, height) \
    [self.card.cardView.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor], \
    [self.card.cardView.centerYAnchor constraintEqualToAnchor:self.view.centerYAnchor], \
    [self.card.cardView.widthAnchor constraintEqualToConstant:width], \
    [self.card.cardView.heightAnchor constraintEqualToConstant:height]

#define popupAlertConstraint(card) \
    [self.card.alertView.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor], \
    [self.card.alertView.centerYAnchor constraintEqualToAnchor:self.view.centerYAnchor], \
    [self.card.alertView.widthAnchor constraintEqualToConstant:280.0f]

#define fillScreenConstraint(_view) \
    [self._view.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor], \
    [self._view.centerYAnchor constraintEqualToAnchor:self.view.centerYAnchor], \
    [self._view.widthAnchor constraintEqualToAnchor:self.view.widthAnchor], \
    [self._view.heightAnchor constraintEqualToAnchor:self.view.heightAnchor]
    
#define mainCardOptionConstraint(option, last, offset) \
    [self.option.optionView.centerXAnchor constraintEqualToAnchor:self.mainCard.cardView.centerXAnchor], \
    [self.option.optionView.widthAnchor constraintEqualToConstant:amethyst.buttonCardWidth-60], \
    [self.option.optionView.heightAnchor constraintEqualToConstant:55], \
    [self.option.optionView.topAnchor constraintEqualToAnchor:self.last.topAnchor constant:10+offset]

#define exploitButtonConstraint(button, top) \
    [self.button.optionView.topAnchor constraintEqualToAnchor:self.top.bottomAnchor constant:20.0f], \
    [self.button.optionView.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor], \
    [self.button.optionView.widthAnchor constraintEqualToAnchor:self.exploitPopupCard.cardView.widthAnchor constant:-40.0f], \
    [self.button.optionView.heightAnchor constraintEqualToConstant:40.0f]

@interface MainController : UIViewController <UITextFieldDelegate, UIPopoverPresentationControllerDelegate>

@property (nonatomic, strong) UILayoutGuide *layoutGuide;
@property (nonatomic, strong) UIImageView *titleIcon;
@property (nonatomic, strong) UILabel *mainTitle;
@property (nonatomic, strong) UILabel *mainSubtitle;
@property (nonatomic, strong) UIImageView *bgImage;
@property (nonatomic, strong) UIVisualEffectView *blurOverlay;
@property (nonatomic, strong) UIView *gestureOverlay;
@property (nonatomic, strong) UIView *alertOverlay;
@property (nonatomic, strong) UILabel *versionLabel;
@property (nonatomic, strong) UIImageView *versionIcon;
@property (nonatomic, strong) UIActivityIndicatorView *statusIndicator;
@property (nonatomic, assign) bool hideStatusBar;
@property (nonatomic, assign) int isJailbroken;
@property (nonatomic, strong) CreditsController *creditsTable;
@property (nonatomic, strong) PopupOption *exploitOption;
@property (nonatomic, strong) PopupOption *toggleOption;
@property (nonatomic, strong) PopupOption *enableTweaksOption;
@property (nonatomic, strong) PopupOption *restoreRootFSOption;
@property (nonatomic, strong) PopupOption *setGeneratorOption;
@property (nonatomic, strong) PopupOption *themeOption;
@property (nonatomic, strong) PopupOption *userspaceRebootOption;
@property (nonatomic, strong) PopupOption *respringOption;
@property (nonatomic, strong) PopupOption *iconCacheOption;
@property (nonatomic, strong) PopupOption *settingsOption1;
@property (nonatomic, strong) PopupOption *settingsOption2;
@property (nonatomic, strong) PopupOption *settingsOption3;
@property (nonatomic, strong) PopupOption *settingsOption4;
@property (nonatomic, strong) PopupOption *settingsOption5;
@property (nonatomic, strong) PopupCard *settingsPopupCard;
@property (nonatomic, strong) PopupCard *creditsPopupCard;
@property (nonatomic, strong) CardView *mainCard;
@property (nonatomic, strong) CardButton *jailbreakCardButton;
@property (nonatomic, strong) CardButton *creditsCardButton;
@property (nonatomic, strong) CardButton *settingsCardButton;
@property (nonatomic, strong) PopupCard *themePopupCard;
@property (nonatomic, strong) PopupCard *generatorPopupCard;
@property (nonatomic, strong) PopupCard *exploitPopupCard;
@property (nonatomic, strong) PopupOption *themeOption1;
@property (nonatomic, strong) PopupOption *themeOption2;
@property (nonatomic, strong) PopupOption *themeOption3;
@property (nonatomic, strong) PopupOption *themeOption4;
@property (nonatomic, strong) PopupOption *lightModeOption;
@property (nonatomic, strong) PopupOption *saveGeneratorOption;
@property (nonatomic, strong) UITextField *generatorTextField;
@property (nonatomic, strong) PopupOption *exploitOptionHemlock;
@property (nonatomic, strong) PopupOption *exploitOptionTrigon;
@property (nonatomic, strong) PopupAlert *alert;
@property (nonatomic, strong) ProgressRing *progressRing;
@property (nonatomic, strong) InstallAlert *installAlert;
@property (nonatomic, strong) LoadingAlert *loadingAlert;
@property (nonatomic, strong) PopupCard *aboutPopupCard;
@property (nonatomic, strong) PopupOption *aboutMeOption;
@property (nonatomic, strong) UILabel *aboutLabel;

- (void)openAboutMe;

@end

#endif

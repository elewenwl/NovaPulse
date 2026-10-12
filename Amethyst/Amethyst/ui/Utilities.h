#ifndef Utilities_h
#define Utilities_h

#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>

@interface UITextField (extensions)
+ (UITextField *)createTextField:(id)_id sel:(SEL)sel text:(NSString *)text;
@end

@interface UIImageView (extensions)
+ (UIImageView *)createImage:(NSString *)name alpha:(CGFloat)alpha;
@end

@interface UILabel (extensions)
+ (UILabel *)createLabel:(NSString *)text size:(CGFloat)size bold:(bool)bold align:(NSTextAlignment)align;
- (void)addShadowWithRadius:(CGFloat)radius opacity:(CGFloat)opacity offset:(CGSize)offset;
@end

@interface UISwitch (extensions)
+ (UISwitch *)createSwitch;
@end

@interface UIButton (extensions)
+ (UIButton *)createButton:(id)_id sel:(SEL)sel text:(NSString *)text;
+ (UIButton *)createOptionButton:(id)_id sel:(SEL)sel text:(NSString *)text;
+ (UIButton *)createImageButton:(id)_id sel:(SEL)sel imageName:(NSString *)imageName;
@end

@interface UIView (extensions)
+ (UIView *)createSeprator:(BOOL)lightMode;
@end


@interface PopupOption: NSObject
@property (nonatomic, strong) UIView *optionView;
@property (nonatomic, strong) UILabel *optionTitle;
@property (nonatomic, strong) UISwitch *optionSwitch;
@property (nonatomic, strong) UIButton *optionButton;
@property (nonatomic, strong) UIView *separator;

- (id)initButtonOption:(NSString *)title _id:(id)_id _sel:(SEL)_sel text:(NSString *)text;
- (id)initImageButtonOption:(NSString *)imageName _id:(id)_id _sel:(SEL)_sel;
- (id)initFullButtonOption:(NSString *)title _id:(id)_id _sel:(SEL)_sel;
- (id)initSwitchOption:(NSString *)title;
- (void)showSeprator:(BOOL)val;
- (BOOL)getSwitchValue;
- (void)setSwitchValue:(BOOL)val;
- (void)setSwitchCallback:(id)_id _sel:(SEL)_sel;
@end



@interface PopupCard: NSObject
@property (nonatomic, strong) UIView *cardView;
@property (nonatomic, strong) UILabel *cardTitle;
@property (nonatomic, strong) UIView *separator;
@property (nonatomic, strong) UIVisualEffectView *blurOverlay;
@property (nonatomic, strong) UIView *gestureOverlay;
@property (nonatomic, assign) BOOL useOverlays;
@property (nonatomic, strong) UIVisualEffectView *lightBlur;
@property (nonatomic, strong) UIVisualEffectView *darkBlur;

- (id)init:(NSString *)title withViews:(NSArray<UIView *>*)views;
- (void)setOverlays:(UIVisualEffectView *)blur gesture:(UIView *)gesture;
- (void)openCard;
- (void)closeCard;
- (BOOL)isOpen;
- (void)switchToCard:(PopupCard *)card;
- (void)openPopover;
- (void)closePopover;
- (void)toggleLightMode;
@end



@interface CardView: NSObject
@property (nonatomic, strong) UIView *cardView;
@property (nonatomic, strong) UIVisualEffectView *lightBlur;
@property (nonatomic, strong) UIVisualEffectView *darkBlur;

- (id)initWithViews:(NSArray<UIView *>*)views;
- (void)toggleLightMode;
@end



@interface CardButton: NSObject
@property (nonatomic, strong) UIView *optionView;
@property (nonatomic, strong) UIButton *optionButton;
@property (nonatomic, strong) UIView *separator;

- (id)init:(NSString *)title _id:(id)_id _sel:(SEL)_sel;
- (void)showSeprator:(BOOL)val;
- (void)setTitle:(NSString *)title;
@end

@interface PopupAlert: NSObject
@property (nonatomic, strong) UIView *alertView;
@property (nonatomic, strong) UIView *alertOverlay;
@property (nonatomic, strong) UILabel *alertTitle;
@property (nonatomic, strong) UILabel *alertMessage;
@property (nonatomic, strong) UIButton *alertButton;
@property (nonatomic, strong) UIView *separator;
@property (nonatomic, strong) UIVisualEffectView *lightBlur;
@property (nonatomic, strong) UIVisualEffectView *darkBlur;

- (id)init:(NSString *)title message:(NSString *)message button:(NSString *)button _id:(id)_id _sel:(SEL)_sel;
- (void)setText:(NSString *)title message:(NSString *)message button:(NSString *)button;
- (void)setAction:(id)_id _sel:(SEL)_sel;
- (void)setOverlay:(UIView *)view;
- (void)openAlert;
- (void)closeAlert;
- (BOOL)isOpen;
- (void)toggleLightMode;
@end


@interface InstallAlert: NSObject
@property (nonatomic, strong) UIView *alertView;
@property (nonatomic, strong) UIView *alertOverlay;
@property (nonatomic, strong) UILabel *alertTitle;
@property (nonatomic, strong) UILabel *alertMessage;
@property (nonatomic, strong) UIButton *alertButton;
@property (nonatomic, strong) PopupOption *installSileo;
@property (nonatomic, strong) PopupOption *installZebra;
@property (nonatomic, strong) PopupOption *installOpenSSH;
@property (nonatomic, strong) UIView *separator;
@property (nonatomic, strong) UIVisualEffectView *lightBlur;
@property (nonatomic, strong) UIVisualEffectView *darkBlur;
@property (nonatomic, assign) BOOL selectionDone;

- (id)init;
- (void)setOverlay:(UIView *)view;
- (void)openAlert;
- (void)closeAlert;
- (BOOL)isOpen;
- (void)toggleLightMode;
@end

@interface LoadingAlert: NSObject
@property (nonatomic, strong) UIView *alertView;
@property (nonatomic, strong) UIView *alertOverlay;
@property (nonatomic, strong) UILabel *alertTitle;
@property (nonatomic, strong) UIView *separator;
@property (nonatomic, strong) UIActivityIndicatorView *indicator;
@property (nonatomic, strong) UIVisualEffectView *lightBlur;
@property (nonatomic, strong) UIVisualEffectView *darkBlur;
@property (nonatomic, assign) BOOL loadingDone;

- (id)init:(NSString *)title;
- (void)setOverlay:(UIView *)view;
- (void)openAlert;
- (void)closeAlert;
- (BOOL)isOpen;
- (void)toggleLightMode;
@end



@interface ProgressRing : UIView
@property (nonatomic, assign) CGFloat currentProgress;
@property (nonatomic, assign) CGFloat progressWidth;
@property (nonatomic, strong) UIColor *progressColor;
@property (nonatomic, strong) UIColor *ringColor;
@property (nonatomic, strong) UILabel *progressLabel;
@property (nonatomic, strong) NSTimer *lastTimer;

- (instancetype)initWithFrame:(CGRect)frame;
- (void)drawRect:(CGRect)rect;
- (void)setProgressPercent:(CGFloat)progress label:(NSString *)label;
- (void)resetProgress;
- (void)showRing;
- (void)hideRing;
@end

#endif /* Utilities_h */

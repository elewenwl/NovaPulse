#include "../AppDelegate.h"
#include "Utilities.h"

@implementation UIImageView (extensions)
+ (UIImageView *)createImage:(NSString *)name alpha:(CGFloat)alpha {
    UIImageView *imageView = [[UIImageView alloc] initWithImage:[UIImage imageNamed:name]];
    imageView.translatesAutoresizingMaskIntoConstraints = NO;
    imageView.alpha = alpha;
    return imageView;
}
@end

@implementation UITextField (extensions)
+ (UITextField *)createTextField:(id)_id sel:(SEL)sel text:(NSString *)text {
    UITextField *textField = [[UITextField alloc] init];
    if (_id != NULL && sel != NULL) {
        [textField addTarget:_id action:sel forControlEvents:UIControlEventEditingChanged];
    }
    
    textField.translatesAutoresizingMaskIntoConstraints = NO;
    [textField setUserInteractionEnabled:YES];
    textField.layer.borderWidth = 1.0;
    textField.layer.cornerRadius = 12.0;
    textField.layer.masksToBounds = YES;

    textField.text = text;
    textField.font = [UIFont systemFontOfSize:16.0f];
    textField.textColor = [[UIColor whiteColor] colorWithAlphaComponent:0.8f];
    textField.layer.borderColor = [[UIColor whiteColor] colorWithAlphaComponent:0.6f].CGColor;
    textField.textAlignment = NSTextAlignmentCenter;
    textField.autocorrectionType = UITextAutocorrectionTypeNo;
    textField.spellCheckingType = UITextSpellCheckingTypeNo;
    textField.smartQuotesType = UITextSmartQuotesTypeNo;
    textField.smartDashesType = UITextSmartDashesTypeNo;
    textField.smartInsertDeleteType = UITextSmartInsertDeleteTypeNo;
    textField.spellCheckingType = UITextSpellCheckingTypeNo;
    return textField;
}
@end

@implementation UILabel (extensions)
+ (UILabel *)createLabel:(NSString *)text size:(CGFloat)size bold:(bool)bold align:(NSTextAlignment)align {
    UILabel *label = [[UILabel alloc] init];
    label.textAlignment = align;
    label.text = text;

    if (bold) label.font = [UIFont boldSystemFontOfSize:size];
    else label.font = [UIFont systemFontOfSize:size];
    
    label.textColor = [[UIColor whiteColor] colorWithAlphaComponent:0.9];
    label.userInteractionEnabled = NO;
    label.translatesAutoresizingMaskIntoConstraints = NO;
    label.autoresizingMask = UIViewAutoresizingFlexibleWidth|UIViewAutoresizingFlexibleHeight;
    return label;
}

+ (UILabel *)createLabel:(NSString *)text size:(CGFloat)size font:(UIFont *)font align:(NSTextAlignment)align {
    UILabel *label = [[UILabel alloc] init];
    label.textAlignment = align;
    label.text = text;
    label.font = font;
    
    label.textColor = [[UIColor whiteColor] colorWithAlphaComponent:0.9];
    label.userInteractionEnabled = NO;
    label.translatesAutoresizingMaskIntoConstraints = NO;
    label.autoresizingMask = UIViewAutoresizingFlexibleWidth|UIViewAutoresizingFlexibleHeight;
    return label;
}

- (void)addShadowWithRadius:(CGFloat)radius opacity:(CGFloat)opacity offset:(CGSize)offset {
    self.layer.shadowOffset = offset;
    self.layer.shadowRadius = radius;
    self.layer.shadowOpacity = opacity;
}
@end

@implementation UISwitch (extensions)
+ (UISwitch *)createSwitch {
    UISwitch *_switch = [[UISwitch alloc] init];
    _switch.userInteractionEnabled = YES;
    _switch.translatesAutoresizingMaskIntoConstraints = NO;
    _switch.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    return _switch;
}
@end

@implementation UIButton (extensions)
+ (UIButton *)createButton:(id)_id sel:(SEL)sel text:(NSString *)text {
    UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem];
    [button setTitle:text forState:UIControlStateNormal];
    [button setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    if ([UIScreen mainScreen].bounds.size.width == 320.0f) {
        button.titleLabel.font = [UIFont systemFontOfSize:16];
    } else {
        button.titleLabel.font = [UIFont systemFontOfSize:18];
    }
    button.translatesAutoresizingMaskIntoConstraints = NO;
    [button addTarget:_id action:sel forControlEvents:UIControlEventTouchUpInside];
    return button;
}

+ (UIButton *)createOptionButton:(id)_id sel:(SEL)sel text:(NSString *)text {
    UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem];
    [button setTitle:text forState:UIControlStateNormal];
    [button setTitleColor:[UIColor lightGrayColor] forState:UIControlStateNormal];
    if ([UIScreen mainScreen].bounds.size.width == 320.0f) {
        button.titleLabel.font = [UIFont systemFontOfSize:12];
    } else {
        button.titleLabel.font = [UIFont systemFontOfSize:14];
    }
    button.translatesAutoresizingMaskIntoConstraints = NO;
    [button addTarget:_id action:sel forControlEvents:UIControlEventTouchUpInside];
    return button;
}

+ (UIButton *)createImageButton:(id)_id sel:(SEL)sel imageName:(NSString *)imageName {
    UIButton *button = [UIButton buttonWithType:UIButtonTypeCustom];
    button.translatesAutoresizingMaskIntoConstraints = NO;
    UIImage *image = [UIImage imageNamed:imageName];
    [button setImage:image forState:UIControlStateNormal];
    [button addTarget:_id action:sel forControlEvents:UIControlEventTouchUpInside];
    return button;
}
@end

@implementation UIView (extensions)
+ (UIView *)createSeprator:(BOOL)lightMode  {
    UIView *separator = [[UIView alloc] init];
    CGFloat alpha = lightMode ? 0.80 : 0.45;
    separator.backgroundColor = [[UIColor lightGrayColor] colorWithAlphaComponent:alpha];
    separator.translatesAutoresizingMaskIntoConstraints = NO;
    return separator;
}
@end



@implementation PopupOption
- (id)initButtonOption:(NSString *)title _id:(id)_id _sel:(SEL)_sel text:(NSString *)text {
    self = [super init];
    self.optionView = [[UIView alloc] init];
    self.optionView.backgroundColor = [UIColor clearColor];
    self.optionView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.optionView setUserInteractionEnabled:YES];
    
    self.optionTitle = [UILabel createLabel:title size:18 bold:0 align:NSTextAlignmentLeft];
    self.optionButton = [UIButton createOptionButton:_id sel:_sel text:text];
    self.separator = [UIView createSeprator:YES];
    [self.optionView addSubview:self.optionTitle];
    [self.optionView addSubview:self.optionButton];
    [self.optionView addSubview:self.separator];

    [NSLayoutConstraint activateConstraints:@[
        [self.optionTitle.leadingAnchor constraintEqualToAnchor:self.optionView.leadingAnchor],
        [self.optionTitle.topAnchor constraintEqualToAnchor:self.optionView.topAnchor],
        [self.optionButton.trailingAnchor constraintEqualToAnchor:self.optionView.trailingAnchor],
        [self.optionButton.centerYAnchor constraintEqualToAnchor:self.optionTitle.centerYAnchor],
        [self.separator.topAnchor constraintEqualToAnchor:self.optionTitle.bottomAnchor constant:20],
        [self.separator.leadingAnchor constraintEqualToAnchor:self.optionView.leadingAnchor],
        [self.separator.trailingAnchor constraintEqualToAnchor:self.optionView.trailingAnchor],
        [self.separator.heightAnchor constraintEqualToConstant:1],
    ]];
    return self;
}

- (id)initSwitchOption:(NSString *)title {
    self = [super init];
    self.optionView = [[UIView alloc] init];
    self.optionView.backgroundColor = [UIColor clearColor];
    self.optionView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.optionView setUserInteractionEnabled:YES];
    
    self.optionTitle = [UILabel createLabel:title size:18 bold:0 align:NSTextAlignmentLeft];
    self.optionSwitch = [UISwitch createSwitch];
    self.separator = [UIView createSeprator:YES];
    [self.optionView addSubview:self.optionTitle];
    [self.optionView addSubview:self.optionSwitch];
    [self.optionView addSubview:self.separator];
    
    self.optionSwitch.thumbTintColor = [[UIColor whiteColor] colorWithAlphaComponent:0.80f];
    self.optionSwitch.onTintColor = [[UIColor blackColor] colorWithAlphaComponent:0.20f];
    self.optionSwitch.tintColor = [[UIColor blackColor] colorWithAlphaComponent:0.20f];
    self.optionSwitch.backgroundColor = [UIColor clearColor];

    self.optionSwitch.layer.borderWidth = 1.0;
    self.optionSwitch.layer.cornerRadius = 16.0;
    self.optionSwitch.layer.borderColor = [[UIColor whiteColor] colorWithAlphaComponent:0.80f].CGColor;

    [NSLayoutConstraint activateConstraints:@[
        [self.optionTitle.leadingAnchor constraintEqualToAnchor:self.optionView.leadingAnchor],
        [self.optionTitle.topAnchor constraintEqualToAnchor:self.optionView.topAnchor],
        [self.optionSwitch.trailingAnchor constraintEqualToAnchor:self.optionView.trailingAnchor],
        [self.optionSwitch.centerYAnchor constraintEqualToAnchor:self.optionTitle.centerYAnchor],
        [self.separator.topAnchor constraintEqualToAnchor:self.optionTitle.bottomAnchor constant:20],
        [self.separator.leadingAnchor constraintEqualToAnchor:self.optionView.leadingAnchor],
        [self.separator.trailingAnchor constraintEqualToAnchor:self.optionView.trailingAnchor],
        [self.separator.heightAnchor constraintEqualToConstant:1],
    ]];
    return self;
}

- (id)initImageButtonOption:(NSString *)imageName _id:(id)_id _sel:(SEL)_sel {
    self = [super init];
    self.optionView = [[UIView alloc] init];
    self.optionView.backgroundColor = [UIColor clearColor];
    self.optionView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.optionView setUserInteractionEnabled:YES];
    
    self.optionButton = [UIButton createImageButton:_id sel:_sel imageName:imageName];
    self.optionButton.layer.borderWidth = 1.0;
    self.optionButton.layer.cornerRadius = 16.0;
    self.optionButton.layer.masksToBounds = YES;
    self.optionButton.layer.borderColor = [[UIColor whiteColor] colorWithAlphaComponent:0.40f].CGColor;
    [self.optionView addSubview:self.optionButton];

    [NSLayoutConstraint activateConstraints:@[
        [self.optionButton.centerXAnchor constraintEqualToAnchor:self.optionView.centerXAnchor],
        [self.optionButton.centerYAnchor constraintEqualToAnchor:self.optionView.centerYAnchor],
        [self.optionButton.topAnchor constraintEqualToAnchor:self.optionView.topAnchor],
        [self.optionButton.bottomAnchor constraintEqualToAnchor:self.optionView.bottomAnchor],
        [self.optionButton.leadingAnchor constraintEqualToAnchor:self.optionView.leadingAnchor],
        [self.optionButton.trailingAnchor constraintEqualToAnchor:self.optionView.trailingAnchor],
        [self.optionView.widthAnchor constraintEqualToConstant:80.0f],
        [self.optionView.heightAnchor constraintEqualToConstant:80.0f],
    ]];
    return self;
}

- (id)initFullButtonOption:(NSString *)title _id:(id)_id _sel:(SEL)_sel {
    self = [super init];
    self.optionView = [[UIView alloc] init];
    self.optionView.backgroundColor = [UIColor clearColor];
    self.optionView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.optionView setUserInteractionEnabled:YES];
    
    self.optionButton = [UIButton createButton:_id sel:_sel text:title];
    self.optionButton.layer.borderWidth = 1.0;
    self.optionButton.layer.cornerRadius = 12.0;
    self.optionButton.layer.masksToBounds = YES;
    self.optionButton.layer.borderColor = [[UIColor whiteColor] colorWithAlphaComponent:0.60f].CGColor;
    [self.optionView addSubview:self.optionButton];

    [NSLayoutConstraint activateConstraints:@[
        [self.optionButton.centerXAnchor constraintEqualToAnchor:self.optionView.centerXAnchor],
        [self.optionButton.centerYAnchor constraintEqualToAnchor:self.optionView.centerYAnchor],
        [self.optionButton.topAnchor constraintEqualToAnchor:self.optionView.topAnchor],
        [self.optionButton.bottomAnchor constraintEqualToAnchor:self.optionView.bottomAnchor],
        [self.optionButton.leadingAnchor constraintEqualToAnchor:self.optionView.leadingAnchor],
        [self.optionButton.trailingAnchor constraintEqualToAnchor:self.optionView.trailingAnchor],
    ]];
    return self;
}

- (void)showSeprator:(BOOL)val {
    if (self.separator != NULL) {
        self.separator.alpha = val ? 1.0f : 0.0f;
    }
}

- (BOOL)getSwitchValue {
    if (self.optionSwitch == NULL) return false;
    return self.optionSwitch.isOn;
}

- (void)setSwitchValue:(BOOL)val {
    if (self.optionSwitch == NULL) return;
    [self.optionSwitch setOn:val];
}

- (void)setSwitchCallback:(id)_id _sel:(SEL)_sel {
    if (self.optionSwitch == NULL) return;
    [self.optionSwitch addTarget:_id action:_sel forControlEvents:UIControlEventValueChanged];
}
@end



@implementation PopupCard
- (id)init:(NSString *)title withViews:(NSArray<UIView *>*)views {
    self = [super init];
    self.cardView = [[UIView alloc] init];
    self.cardView.translatesAutoresizingMaskIntoConstraints = NO;
    self.cardView.transform = CGAffineTransformScale(CGAffineTransformMakeTranslation(0, 0), 2.0, 2.0);
    self.cardView.layer.borderColor = [[UIColor whiteColor] colorWithAlphaComponent:0.1].CGColor;
    self.cardView.layer.cornerRadius = 20;
    self.cardView.layer.masksToBounds = YES;
    self.cardView.layer.borderWidth = 1.0;
    self.cardView.alpha = 0.0;
    
    self.lightBlur = [[UIVisualEffectView alloc] initWithEffect:[UIBlurEffect effectWithStyle:UIBlurEffectStyleLight]];
    self.darkBlur = [[UIVisualEffectView alloc] initWithEffect:[UIBlurEffect effectWithStyle:UIBlurEffectStyleDark]];
    
    self.lightBlur.userInteractionEnabled = NO;
    self.lightBlur.frame = self.cardView.bounds;
    self.lightBlur.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.darkBlur.userInteractionEnabled = NO;
    self.darkBlur.frame = self.cardView.bounds;
    self.darkBlur.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    
    if (amethyst.lightMode) {
        self.lightBlur.alpha = 1.0f;
        self.darkBlur.alpha = 0.0f;
    } else {
        self.lightBlur.alpha = 0.0f;
        self.darkBlur.alpha = 1.0f;
    }
    
    [self.cardView addSubview:self.lightBlur];
    [self.cardView addSubview:self.darkBlur];
    [self.cardView setUserInteractionEnabled:YES];

    self.cardTitle = [UILabel createLabel:title size:20 bold:1 align:NSTextAlignmentCenter];
    self.separator = [UIView createSeprator:YES];
    [self.cardView addSubview:self.cardTitle];
    [self.cardView addSubview:self.separator];
    self.useOverlays = NO;

    for (id view in views) {
        if (view != NULL) [self.cardView addSubview:view];
    }
    
    [NSLayoutConstraint activateConstraints:@[
        [self.cardTitle.centerXAnchor constraintEqualToAnchor:self.cardView.centerXAnchor],
        [self.cardTitle.topAnchor constraintEqualToAnchor:self.cardView.topAnchor constant:20],
        
        [self.separator.topAnchor constraintEqualToAnchor:self.cardTitle.bottomAnchor constant:15],
        [self.separator.leadingAnchor constraintEqualToAnchor:self.cardView.leadingAnchor constant:20],
        [self.separator.trailingAnchor constraintEqualToAnchor:self.cardView.trailingAnchor constant:-20],
        [self.separator.heightAnchor constraintEqualToConstant:1],
    ]];
    return self;
}

- (void)toggleLightMode {
    if (amethyst.lightMode) {
        [UIView animateWithDuration:0.3 animations:^{
            self.lightBlur.alpha = 1.0f;
            self.darkBlur.alpha = 0.0f;
        }];
    } else {
        [UIView animateWithDuration:0.3 animations:^{
            self.lightBlur.alpha = 0.0f;
            self.darkBlur.alpha = 1.0f;
        }];
    }
}

- (void)setOverlays:(UIVisualEffectView *)blur gesture:(UIView *)gesture {
    self.blurOverlay = blur;
    self.gestureOverlay = gesture;
    self.useOverlays = YES;
}

- (void)openCard {
    [UIView animateWithDuration:0.3 animations:^{
        self.cardView.alpha = 0.95;
        self.cardView.transform = CGAffineTransformScale(CGAffineTransformMakeTranslation(0, 0), 1.0, 1.0);
        if (self.useOverlays) self.blurOverlay.alpha = 1.0;
    }];
    if (self.useOverlays) self.gestureOverlay.userInteractionEnabled = YES;

}

- (void)closeCard {
    [UIView animateWithDuration:0.3 animations:^{
        self.cardView.alpha = 0.0;
        self.cardView.transform = CGAffineTransformScale(CGAffineTransformMakeTranslation(0, 0), 1.4, 1.4);
        if (self.useOverlays) self.blurOverlay.alpha = 0;
    }];
    if (self.useOverlays) self.gestureOverlay.userInteractionEnabled = NO;
}

- (void)openPopover{
    [UIView animateWithDuration:0.3 animations:^{
        self.cardView.alpha = 0.95;
        self.cardView.transform = CGAffineTransformScale(CGAffineTransformMakeTranslation(0, 0), 1.0, 1.0);
        if (self.useOverlays) self.blurOverlay.alpha = 1.0;
    }];
    if (self.useOverlays) self.gestureOverlay.userInteractionEnabled = YES;

}

- (void)closePopover {
    [UIView animateWithDuration:0.3 animations:^{
        self.cardView.alpha = 0.0;
        self.cardView.transform = CGAffineTransformScale(CGAffineTransformMakeTranslation(0, 0), 1.4, 1.4);
        if (self.useOverlays) self.blurOverlay.alpha = 0;
    }];
    if (self.useOverlays) self.gestureOverlay.userInteractionEnabled = NO;
}

- (BOOL)isOpen {
    return (self.cardView.alpha != 0.0);
}

- (void)switchToCard:(PopupCard *)card {
    [UIView animateWithDuration:0.27 animations:^{
        self.cardView.alpha = 0.0;
        self.cardView.transform = CGAffineTransformScale(CGAffineTransformMakeTranslation(0, 0), 0.9, 0.9);
    }];
    
    [UIView animateWithDuration:0.27 animations:^{
        card.cardView.alpha = 0.95;
        card.cardView.transform = CGAffineTransformScale(CGAffineTransformMakeTranslation(0, 0), 1.0, 1.0);
    }];

    if (self.useOverlays) self.gestureOverlay.userInteractionEnabled = NO;
    if (card.useOverlays) card.gestureOverlay.userInteractionEnabled = YES;
}
@end


@implementation CardView
- (id)initWithViews:(NSArray<UIView *>*)views {
    self = [super init];
    self.cardView = [[UIView alloc] init];
    self.cardView.translatesAutoresizingMaskIntoConstraints = NO;
    self.cardView.layer.borderColor = [[UIColor whiteColor] colorWithAlphaComponent:0.1].CGColor;
    self.cardView.layer.cornerRadius = 20;
    self.cardView.layer.masksToBounds = YES;
    self.cardView.layer.borderWidth = 1.0;
    
    self.lightBlur = [[UIVisualEffectView alloc] initWithEffect:[UIBlurEffect effectWithStyle:UIBlurEffectStyleLight]];
    self.darkBlur = [[UIVisualEffectView alloc] initWithEffect:[UIBlurEffect effectWithStyle:UIBlurEffectStyleDark]];
    
    self.lightBlur.userInteractionEnabled = NO;
    self.lightBlur.frame = self.cardView.bounds;
    self.lightBlur.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.darkBlur.userInteractionEnabled = NO;
    self.darkBlur.frame = self.cardView.bounds;
    self.darkBlur.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    
    if (amethyst.lightMode) {
        self.lightBlur.alpha = 1.0f;
        self.darkBlur.alpha = 0.0f;
    } else {
        self.lightBlur.alpha = 0.0f;
        self.darkBlur.alpha = 1.0f;
    }
    
    [self.cardView addSubview:self.lightBlur];
    [self.cardView addSubview:self.darkBlur];
    [self.cardView setUserInteractionEnabled:YES];
    
    for (id view in views) {
        if (view != NULL) [self.cardView addSubview:view];
    }
    return self;
}

- (void)toggleLightMode {
    if (amethyst.lightMode) {
        [UIView animateWithDuration:0.3 animations:^{
            self.lightBlur.alpha = 1.0f;
            self.darkBlur.alpha = 0.0f;
        }];
    } else {
        [UIView animateWithDuration:0.3 animations:^{
            self.lightBlur.alpha = 0.0f;
            self.darkBlur.alpha = 1.0f;
        }];
    }
}
@end


@implementation CardButton
- (id)init:(NSString *)title _id:(id)_id _sel:(SEL)_sel {
    self = [super init];
    self.optionView = [[UIView alloc] init];
    self.optionView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.optionView setUserInteractionEnabled:YES];
    self.separator = [UIView createSeprator:YES];

    self.optionButton = [UIButton buttonWithType:UIButtonTypeSystem];
    [self.optionButton setTitle:title forState:UIControlStateNormal];
    [self.optionButton setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    self.optionButton.titleLabel.font = [UIFont systemFontOfSize:18];
    self.optionButton.translatesAutoresizingMaskIntoConstraints = NO;
    [self.optionButton addTarget:_id action:_sel forControlEvents:UIControlEventTouchUpInside];
    
    [self.optionView addSubview:self.optionButton];
    [self.optionView addSubview:self.separator];
    
    [NSLayoutConstraint activateConstraints:@[
        [self.optionButton.leadingAnchor constraintEqualToAnchor:self.optionView.leadingAnchor],
        [self.optionButton.trailingAnchor constraintEqualToAnchor:self.optionView.trailingAnchor],
        [self.optionButton.topAnchor constraintEqualToAnchor:self.optionView.topAnchor],

        [self.separator.topAnchor constraintEqualToAnchor:self.optionButton.bottomAnchor constant:10],
        [self.separator.leadingAnchor constraintEqualToAnchor:self.optionButton.leadingAnchor],
        [self.separator.trailingAnchor constraintEqualToAnchor:self.optionButton.trailingAnchor],
        [self.separator.heightAnchor constraintEqualToConstant:1],
    ]];
    return self;
}

- (void)showSeprator:(BOOL)val {
    if (self.separator != NULL) {
        self.separator.alpha = val ? 1.0f : 0.0f;
    }
}

- (void)setTitle:(NSString *)title {
    [self.optionButton setTitle:title forState:UIControlStateNormal];
}
@end


@implementation PopupAlert: NSObject
- (id)init:(NSString *)title message:(NSString *)message button:(NSString *)button _id:(id)_id _sel:(SEL)_sel {
    self = [super init];
    self.alertView = [[UIView alloc] init];
    self.alertView.translatesAutoresizingMaskIntoConstraints = NO;
    self.alertView.transform = CGAffineTransformScale(CGAffineTransformMakeTranslation(0, 0), 2.0, 2.0);
    self.alertView.layer.borderColor = [[UIColor whiteColor] colorWithAlphaComponent:0.1].CGColor;
    self.alertView.layer.cornerRadius = 20;
    self.alertView.layer.masksToBounds = YES;
    self.alertView.layer.borderWidth = 1.0;
    self.alertView.alpha = 0.0;
    
    self.lightBlur = [[UIVisualEffectView alloc] initWithEffect:[UIBlurEffect effectWithStyle:UIBlurEffectStyleLight]];
    self.darkBlur = [[UIVisualEffectView alloc] initWithEffect:[UIBlurEffect effectWithStyle:UIBlurEffectStyleDark]];
    
    self.lightBlur.userInteractionEnabled = NO;
    self.lightBlur.frame = self.alertView.bounds;
    self.lightBlur.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.darkBlur.userInteractionEnabled = NO;
    self.darkBlur.frame = self.alertView.bounds;
    self.darkBlur.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    
    if (amethyst.lightMode) {
        self.lightBlur.alpha = 1.0f;
        self.darkBlur.alpha = 0.0f;
    } else {
        self.lightBlur.alpha = 0.0f;
        self.darkBlur.alpha = 1.0f;
    }

    [self.alertView addSubview:self.lightBlur];
    [self.alertView addSubview:self.darkBlur];
    [self.alertView setUserInteractionEnabled:YES];

    self.alertTitle = [UILabel createLabel:title size:20 bold:YES align:NSTextAlignmentCenter];
    self.alertMessage = [UILabel createLabel:message size:14 bold:NO align:NSTextAlignmentCenter];
    self.alertMessage.numberOfLines = 0;

    self.separator = [UIView createSeprator:YES];
    [self.alertView addSubview:self.alertTitle];
    [self.alertView addSubview:self.alertMessage];
    [self.alertView addSubview:self.separator];
    
    self.alertButton = [UIButton createButton:_id sel:_sel text:button];
    self.alertButton.layer.borderWidth = 1.0;
    self.alertButton.layer.cornerRadius = 12.0;
    self.alertButton.layer.masksToBounds = YES;
    self.alertButton.layer.borderColor = [[UIColor whiteColor] colorWithAlphaComponent:0.60f].CGColor;
    [self.alertView addSubview:self.alertButton];

    [NSLayoutConstraint activateConstraints:@[
        [self.alertTitle.centerXAnchor constraintEqualToAnchor:self.alertView.centerXAnchor],
        [self.alertTitle.topAnchor constraintEqualToAnchor:self.alertView.topAnchor constant:20],
    
        [self.separator.topAnchor constraintEqualToAnchor:self.alertTitle.bottomAnchor constant:10],
        [self.separator.leadingAnchor constraintEqualToAnchor:self.alertView.leadingAnchor constant:20],
        [self.separator.trailingAnchor constraintEqualToAnchor:self.alertView.trailingAnchor constant:-20],
        [self.separator.heightAnchor constraintEqualToConstant:1],
        
        [self.alertMessage.centerXAnchor constraintEqualToAnchor:self.alertView.centerXAnchor],
        [self.alertMessage.topAnchor constraintEqualToAnchor:self.separator.bottomAnchor constant:10],
        [self.alertMessage.leadingAnchor constraintEqualToAnchor:self.alertView.leadingAnchor constant:20],
        [self.alertMessage.trailingAnchor constraintEqualToAnchor:self.alertView.trailingAnchor constant:-20],
        
        [self.alertButton.topAnchor constraintEqualToAnchor:self.alertMessage.bottomAnchor constant:20],
        [self.alertButton.bottomAnchor constraintEqualToAnchor:self.alertView.bottomAnchor constant:-20],

        [self.alertButton.leadingAnchor constraintEqualToAnchor:self.alertView.leadingAnchor constant:20],
        [self.alertButton.trailingAnchor constraintEqualToAnchor:self.alertView.trailingAnchor constant:-20],
        [self.alertButton.heightAnchor constraintEqualToConstant:40],
    ]];
    return self;
}

- (void)toggleLightMode {
    if (amethyst.lightMode) {
        [UIView animateWithDuration:0.3 animations:^{
            self.lightBlur.alpha = 1.0f;
            self.darkBlur.alpha = 0.0f;
        }];
    } else {
        [UIView animateWithDuration:0.3 animations:^{
            self.lightBlur.alpha = 0.0f;
            self.darkBlur.alpha = 1.0f;
        }];
    }
}

- (void)setText:(NSString *)title message:(NSString *)message button:(NSString *)button {
    self.alertTitle.text = title;
    self.alertMessage.text = message;
    [self.alertButton setTitle:button forState:UIControlStateNormal];
}

- (void)setAction:(id)_id _sel:(SEL)_sel {
    [self.alertButton removeTarget:NULL action:NULL forControlEvents:UIControlEventAllEvents];
    [self.alertButton addTarget:_id action:_sel forControlEvents:UIControlEventTouchUpInside];
}

- (void)setOverlay:(UIView *)view {
    self.alertOverlay = view;
    self.alertOverlay.alpha = 0.0f;
}

- (void)openAlert {
    [UIView animateWithDuration:0.3 animations:^{
        self.alertView.alpha = 0.95;
        self.alertView.transform = CGAffineTransformScale(CGAffineTransformMakeTranslation(0, 0), 1.0, 1.0);
        if (self.alertOverlay != NULL) self.alertOverlay.alpha = 0.65f;
    }];
}

- (void)closeAlert {
    [UIView animateWithDuration:0.3 animations:^{
        self.alertView.alpha = 0.0;
        self.alertView.transform = CGAffineTransformScale(CGAffineTransformMakeTranslation(0, 0), 1.4, 1.4);
        if (self.alertOverlay != NULL) self.alertOverlay.alpha = 0.0f;
    }];
}

- (BOOL)isOpen {
    return (self.alertView.alpha != 0.0);
}
@end

@implementation InstallAlert: NSObject
- (id)init {
    self = [super init];
    self.alertView = [[UIView alloc] init];
    self.alertView.translatesAutoresizingMaskIntoConstraints = NO;
    self.alertView.transform = CGAffineTransformScale(CGAffineTransformMakeTranslation(0, 0), 2.0, 2.0);
    self.alertView.layer.borderColor = [[UIColor whiteColor] colorWithAlphaComponent:0.1].CGColor;
    self.alertView.layer.cornerRadius = 20;
    self.alertView.layer.masksToBounds = YES;
    self.alertView.layer.borderWidth = 1.0;
    self.alertView.alpha = 0.0;
    
    self.lightBlur = [[UIVisualEffectView alloc] initWithEffect:[UIBlurEffect effectWithStyle:UIBlurEffectStyleLight]];
    self.darkBlur = [[UIVisualEffectView alloc] initWithEffect:[UIBlurEffect effectWithStyle:UIBlurEffectStyleDark]];
    
    self.lightBlur.userInteractionEnabled = NO;
    self.lightBlur.frame = self.alertView.bounds;
    self.lightBlur.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.darkBlur.userInteractionEnabled = NO;
    self.darkBlur.frame = self.alertView.bounds;
    self.darkBlur.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    
    if (amethyst.lightMode) {
        self.lightBlur.alpha = 1.0f;
        self.darkBlur.alpha = 0.0f;
    } else {
        self.lightBlur.alpha = 0.0f;
        self.darkBlur.alpha = 1.0f;
    }

    [self.alertView addSubview:self.lightBlur];
    [self.alertView addSubview:self.darkBlur];
    [self.alertView setUserInteractionEnabled:YES];

    self.alertTitle = [UILabel createLabel:@"Configure Install" size:20 bold:YES align:NSTextAlignmentCenter];
    self.separator = [UIView createSeprator:YES];
    [self.alertView addSubview:self.alertTitle];
    [self.alertView addSubview:self.alertMessage];
    [self.alertView addSubview:self.separator];
    
    self.alertButton = [UIButton createButton:self sel:@selector(closeAlert) text:@"Okay"];
    self.alertButton.layer.borderWidth = 1.0;
    self.alertButton.layer.cornerRadius = 12.0;
    self.alertButton.layer.masksToBounds = YES;
    self.alertButton.layer.borderColor = [[UIColor whiteColor] colorWithAlphaComponent:0.60f].CGColor;
    [self.alertButton setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    self.alertButton.enabled = YES;
    [self.alertView addSubview:self.alertButton];

    self.installSileo = [[PopupOption alloc] initSwitchOption:@"Install Sileo"];
    self.installZebra = [[PopupOption alloc] initSwitchOption:@"Install Zebra"];
    [self.installSileo setSwitchValue:true];
    [self.installZebra setSwitchValue:true];
    [self.installSileo showSeprator:NO];
    [self.installZebra showSeprator:NO];
    
    [self.alertView addSubview:self.installSileo.optionView];
    [self.alertView addSubview:self.installZebra.optionView];
    [self.installSileo.optionSwitch addTarget:self action:@selector(switchAction:) forControlEvents:UIControlEventValueChanged];
    [self.installZebra.optionSwitch addTarget:self action:@selector(switchAction:) forControlEvents:UIControlEventValueChanged];

    [NSLayoutConstraint activateConstraints:@[
        [self.alertTitle.centerXAnchor constraintEqualToAnchor:self.alertView.centerXAnchor],
        [self.alertTitle.topAnchor constraintEqualToAnchor:self.alertView.topAnchor constant:20],
        [self.separator.topAnchor constraintEqualToAnchor:self.alertTitle.bottomAnchor constant:10],
        [self.separator.leadingAnchor constraintEqualToAnchor:self.alertView.leadingAnchor constant:20],
        [self.separator.trailingAnchor constraintEqualToAnchor:self.alertView.trailingAnchor constant:-20],
        [self.separator.heightAnchor constraintEqualToConstant:1],
        [self.installSileo.optionView.topAnchor constraintEqualToAnchor:self.separator.bottomAnchor constant:30],
        [self.installSileo.optionView.widthAnchor constraintEqualToAnchor:self.alertView.widthAnchor constant:-40],
        [self.installSileo.optionView.centerXAnchor constraintEqualToAnchor:self.alertView.centerXAnchor],
        [self.installSileo.optionView.heightAnchor constraintEqualToConstant:40],
        [self.installZebra.optionView.topAnchor constraintEqualToAnchor:self.installSileo.optionView.bottomAnchor constant:10],
        [self.installZebra.optionView.widthAnchor constraintEqualToAnchor:self.alertView.widthAnchor constant:-40],
        [self.installZebra.optionView.centerXAnchor constraintEqualToAnchor:self.alertView.centerXAnchor],
        [self.installZebra.optionView.heightAnchor constraintEqualToConstant:40],

        [self.alertButton.topAnchor constraintEqualToAnchor:self.installZebra.optionView.bottomAnchor constant:15],
        [self.alertButton.bottomAnchor constraintEqualToAnchor:self.alertView.bottomAnchor constant:-20],
        [self.alertButton.leadingAnchor constraintEqualToAnchor:self.alertView.leadingAnchor constant:20],
        [self.alertButton.trailingAnchor constraintEqualToAnchor:self.alertView.trailingAnchor constant:-20],
        [self.alertButton.heightAnchor constraintEqualToConstant:40],
    ]];
    return self;
}

- (void)switchAction:(UISwitch *)toggle {
    if (![self.installSileo getSwitchValue] && ![self.installZebra getSwitchValue]) {
        [self.alertButton setTitleColor:[[UIColor whiteColor] colorWithAlphaComponent:0.4f] forState:UIControlStateNormal];
        self.alertButton.enabled = NO;
    } else {
        [self.alertButton setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
        self.alertButton.enabled = YES;
    }
}

- (void)toggleLightMode {
    if (amethyst.lightMode) {
        [UIView animateWithDuration:0.3 animations:^{
            self.lightBlur.alpha = 1.0f;
            self.darkBlur.alpha = 0.0f;
        }];
    } else {
        [UIView animateWithDuration:0.3 animations:^{
            self.lightBlur.alpha = 0.0f;
            self.darkBlur.alpha = 1.0f;
        }];
    }
}

- (void)setOverlay:(UIView *)view {
    self.alertOverlay = view;
    self.alertOverlay.alpha = 0.0f;
}

- (void)openAlert {
    [UIView animateWithDuration:0.3 animations:^{
        self.alertView.alpha = 0.95;
        self.alertView.transform = CGAffineTransformScale(CGAffineTransformMakeTranslation(0, 0), 1.0, 1.0);
        if (self.alertOverlay != NULL) self.alertOverlay.alpha = 0.65f;
    }];
    self.selectionDone = NO;
}

- (void)closeAlert {
    [UIView animateWithDuration:0.3 animations:^{
        self.alertView.alpha = 0.0;
        self.alertView.transform = CGAffineTransformScale(CGAffineTransformMakeTranslation(0, 0), 1.4, 1.4);
        if (self.alertOverlay != NULL) self.alertOverlay.alpha = 0.0f;
    }];
    self.selectionDone = YES;
}

- (BOOL)isOpen {
    return (self.alertView.alpha != 0.0);
}
@end


@implementation LoadingAlert: NSObject
- (id)init:(NSString *)title {
    self = [super init];
    self.alertView = [[UIView alloc] init];
    self.alertView.translatesAutoresizingMaskIntoConstraints = NO;
    self.alertView.transform = CGAffineTransformScale(CGAffineTransformMakeTranslation(0, 0), 2.0, 2.0);
    self.alertView.layer.borderColor = [[UIColor whiteColor] colorWithAlphaComponent:0.1].CGColor;
    self.alertView.layer.cornerRadius = 20;
    self.alertView.layer.masksToBounds = YES;
    self.alertView.layer.borderWidth = 1.0;
    self.alertView.alpha = 0.0;
    
    self.lightBlur = [[UIVisualEffectView alloc] initWithEffect:[UIBlurEffect effectWithStyle:UIBlurEffectStyleLight]];
    self.darkBlur = [[UIVisualEffectView alloc] initWithEffect:[UIBlurEffect effectWithStyle:UIBlurEffectStyleDark]];
    
    self.lightBlur.userInteractionEnabled = NO;
    self.lightBlur.frame = self.alertView.bounds;
    self.lightBlur.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.darkBlur.userInteractionEnabled = NO;
    self.darkBlur.frame = self.alertView.bounds;
    self.darkBlur.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    
    if (amethyst.lightMode) {
        self.lightBlur.alpha = 1.0f;
        self.darkBlur.alpha = 0.0f;
    } else {
        self.lightBlur.alpha = 0.1f;
        self.darkBlur.alpha = 0.9f;
    }

    [self.alertView addSubview:self.lightBlur];
    [self.alertView addSubview:self.darkBlur];
    [self.alertView setUserInteractionEnabled:YES];

    self.alertTitle = [UILabel createLabel:title size:20 bold:YES align:NSTextAlignmentCenter];
    self.separator = [UIView createSeprator:YES];
    [self.alertView addSubview:self.alertTitle];
    [self.alertView addSubview:self.separator];
    
    self.indicator = [[UIActivityIndicatorView alloc] initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleWhite];
    self.indicator.translatesAutoresizingMaskIntoConstraints = NO;
    self.indicator.hidesWhenStopped = true;
    self.indicator.color = [UIColor whiteColor];
    [self.alertView addSubview:self.indicator];

    [NSLayoutConstraint activateConstraints:@[
        [self.alertTitle.centerXAnchor constraintEqualToAnchor:self.alertView.centerXAnchor],
        [self.alertTitle.topAnchor constraintEqualToAnchor:self.alertView.topAnchor constant:20],
        [self.separator.topAnchor constraintEqualToAnchor:self.alertTitle.bottomAnchor constant:10],
        [self.separator.leadingAnchor constraintEqualToAnchor:self.alertView.leadingAnchor constant:20],
        [self.separator.trailingAnchor constraintEqualToAnchor:self.alertView.trailingAnchor constant:-20],
        [self.separator.heightAnchor constraintEqualToConstant:1],
        [self.indicator.topAnchor constraintEqualToAnchor:self.separator.bottomAnchor constant:15],
        [self.indicator.bottomAnchor constraintEqualToAnchor:self.alertView.bottomAnchor constant:-20],
        [self.indicator.leadingAnchor constraintEqualToAnchor:self.alertView.leadingAnchor constant:20],
        [self.indicator.trailingAnchor constraintEqualToAnchor:self.alertView.trailingAnchor constant:-20],
        [self.indicator.heightAnchor constraintEqualToConstant:40],
    ]];
    return self;
}

- (void)toggleLightMode {
    if (amethyst.lightMode) {
        [UIView animateWithDuration:0.3 animations:^{
            self.lightBlur.alpha = 1.0f;
            self.darkBlur.alpha = 0.0f;
        }];
    } else {
        [UIView animateWithDuration:0.3 animations:^{
            self.lightBlur.alpha = 0.1f;
            self.darkBlur.alpha = 0.9f;
        }];
    }
}

- (void)setOverlay:(UIView *)view {
    self.alertOverlay = view;
    self.alertOverlay.alpha = 0.0f;
}

- (void)openAlert {
    [UIView animateWithDuration:0.3 animations:^{
        self.alertView.alpha = 0.98;
        self.alertView.transform = CGAffineTransformScale(CGAffineTransformMakeTranslation(0, 0), 1.0, 1.0);
        if (self.alertOverlay != NULL) self.alertOverlay.alpha = 0.50f;
    }];
    
    [self.indicator startAnimating];
    self.loadingDone = NO;
}

- (void)closeAlert {
    [UIView animateWithDuration:0.3 animations:^{
        self.alertView.alpha = 0.0;
        self.alertView.transform = CGAffineTransformScale(CGAffineTransformMakeTranslation(0, 0), 1.4, 1.4);
        if (self.alertOverlay != NULL) self.alertOverlay.alpha = 0.0f;
    } completion:^(BOOL finished){
        [self.indicator stopAnimating];
    }];
    self.loadingDone = YES;
}

- (BOOL)isOpen {
    return (self.alertView.alpha != 0.0);
}
@end



@implementation ProgressRing
- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    self.translatesAutoresizingMaskIntoConstraints = NO;
    self.transform = CGAffineTransformScale(CGAffineTransformMakeTranslation(0, 0), 0.3, 0.3);
    self.backgroundColor = [UIColor clearColor];
    self.alpha = 0.0f;
    
    self.progressLabel = [[UILabel alloc] init];
    self.progressLabel.textAlignment = NSTextAlignmentCenter;
    self.progressLabel.text = @"Ready";
    self.progressLabel.font = [UIFont systemFontOfSize:15];
    self.progressLabel.textColor = [[UIColor whiteColor] colorWithAlphaComponent:0.95];
    self.progressLabel.userInteractionEnabled = NO;
    self.progressLabel.translatesAutoresizingMaskIntoConstraints = NO;
    self.progressLabel.autoresizingMask = UIViewAutoresizingFlexibleWidth|UIViewAutoresizingFlexibleHeight;

    self.lastTimer = NULL;
    self.currentProgress = 0;
    self.progressWidth = 12;
    self.ringColor = [[UIColor whiteColor] colorWithAlphaComponent:0.2];
    self.progressColor = [[UIColor whiteColor] colorWithAlphaComponent:0.6];

    [self addSubview:self.progressLabel];
    [NSLayoutConstraint activateConstraints:@[
        [self.progressLabel.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
        [self.progressLabel.centerYAnchor constraintEqualToAnchor:self.centerYAnchor]
    ]];
    return self;
}

- (void)drawRect:(CGRect)rect {
    CGContextRef context = UIGraphicsGetCurrentContext();
    CGPoint center = CGPointMake(CGRectGetMidX(rect), CGRectGetMidY(rect));
    CGFloat radius = MIN(CGRectGetWidth(rect), CGRectGetHeight(rect)) / 2 - self.progressWidth / 2;
    
    CGContextSetLineWidth(context, self.progressWidth - 4);
    CGContextSetStrokeColorWithColor(context, self.ringColor.CGColor);
    CGContextAddArc(context, center.x, center.y, radius, 0, 2 * M_PI, 0);
    CGContextStrokePath(context);
    
    CGContextSetLineWidth(context, self.progressWidth);
    CGContextSetLineCap(context, kCGLineCapRound);
    CGContextSetStrokeColorWithColor(context, self.progressColor .CGColor);
    CGFloat startAngle = -M_PI_2;
    CGFloat endAngle = startAngle + 2 * M_PI * self.currentProgress;
    CGContextAddArc(context, center.x, center.y, radius, startAngle, endAngle, 0);
    CGContextStrokePath(context);
}

- (void)setProgressPercent:(CGFloat)progress label:(NSString *)label {
    CGFloat targetProgress = MAX(0.0, MIN(1.0, progress));
    CGFloat cachedProgress = self.currentProgress;
    CGFloat interval = targetProgress <= 0.0 ? 0.001 : 0.02;
    if ((targetProgress - cachedProgress) >= 0.15) interval = 0.01;
    if (self.lastTimer != NULL) [self.lastTimer invalidate];
    self.progressLabel.text = label;
    
    [NSTimer scheduledTimerWithTimeInterval:interval repeats:YES block:^(NSTimer * _Nonnull timer) {
        self.lastTimer = timer;
        if (targetProgress <= 0.0) {
            self.currentProgress -= cachedProgress / 100.0;
            if (self.currentProgress < 0.0) self.currentProgress = 0.0;
        } else {
            self.currentProgress += targetProgress / 100.0;
        }
        
        [self setNeedsDisplay];
        if (targetProgress <= 0.0) {
            if (self.currentProgress <= 0.0) {
                self.lastTimer = NULL;
                [timer invalidate];
            }
        } else {
            if (self.currentProgress >= targetProgress) {
                self.lastTimer = NULL;
                [timer invalidate];
            }
        }
    }];
}

- (void)resetProgress {
    self.progressLabel.text = @"Ready";
    self.currentProgress = 0.0;
    if (self.lastTimer != NULL) {
        [self.lastTimer invalidate];
        self.lastTimer = NULL;
    }
    [self setNeedsDisplay];
}

- (void)showRing {
    [UIView animateWithDuration:0.4 delay:0.0 options:UIViewAnimationOptionCurveEaseInOut animations: ^{
        self.transform = CGAffineTransformScale(CGAffineTransformMakeTranslation(0, 0), 1, 1);
        self.alpha = 0.95;
    } completion: NULL];
}

- (void)hideRing {
    [UIView animateWithDuration:0.4 delay:0.0 options:UIViewAnimationOptionCurveEaseInOut animations: ^{
        self.transform = CGAffineTransformScale(CGAffineTransformMakeTranslation(0, 0), 0.3, 0.3);
        self.alpha = 0.0;
    } completion: ^(BOOL finished) {
        if (finished) [self resetProgress];
    }];
}
@end

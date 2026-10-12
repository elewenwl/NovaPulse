#ifndef CreditsController_h
#define CreditsController_h

#import <UIKit/UIKit.h>

@interface CreditsController : UITableViewController <UITableViewDataSource, UITableViewDelegate>
@property (strong, nonatomic) NSArray *tableData;
@end

@interface CreditsInfo: NSObject
@property (nonatomic, strong) NSString *userName;
@property (nonatomic, strong) NSString *userDetail;
@property (nonatomic, strong) NSString *linkUrl;

- (id)init:(NSString *)name detail:(NSString *)detail linkUrl:(NSString *)linkUrl;
@end

#endif /* CreditsController_h */

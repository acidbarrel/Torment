#import <Cocoa/Cocoa.h>

@class ConsoleView;

@interface Launcher : NSObject
{
    IBOutlet NSPopUpButton *resolutions;
    IBOutlet NSButton *fullscreen;
    IBOutlet NSSlider *shader;
	IBOutlet NSSlider *fsaa;
	IBOutlet NSButton *multiplayer;
	IBOutlet ConsoleView *console;
	IBOutlet NSTextField *serverOptions;
	IBOutlet NSTextField *advancedOptions;
	

	IBOutlet NSTextField *name;
	IBOutlet NSTextField *team;
	
	IBOutlet NSArrayController *keys;
@private
	pid_t server;
}

- (IBAction)playAction:(id)sender;

- (IBAction)multiplayerAction:(id)sender;

- (IBAction)helpAction:(id)sender;

- (IBAction)playRpg:(id)sender;

@end
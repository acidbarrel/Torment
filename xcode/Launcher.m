#import "Launcher.h"
#import "ConsoleView.h"

#include <ApplicationServices/ApplicationServices.h>
#include <stdlib.h>
#include <unistd.h> /* unlink */
#include <util.h> /* forkpty */

#define kMaxDisplays	16

static int numberForKey(CFDictionaryRef desc, CFStringRef key)
{
    CFNumberRef value;
    int num = 0;
	
    if ( (value = CFDictionaryGetValue(desc, key)) == NULL )
        return 0;
	
    CFNumberGetValue(value, kCFNumberIntType, &num);
	
    return num;
}


@implementation Launcher

/* directory where the executable lives */
-(NSString *)cwd {
	return [[[[NSBundle mainBundle] bundlePath] stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"sauerbraten"];
}

/* build key array from config data */
-(NSArray *)getKeys:(NSDictionary *)dict 
{	
	NSMutableArray *arr = [[NSMutableArray alloc] init];
	NSEnumerator *e = [dict keyEnumerator];
	NSString *key;
	while ((key = [e nextObject])) 
	{
		NSString *trig;
		if([key hasPrefix:@"editbind"]) 
			trig = [key substringFromIndex:9];
		else if([key hasPrefix:@"bind"]) 
			trig = [key substringFromIndex:5];
		else 
			continue;
		[arr addObject:[NSDictionary dictionaryWithObjectsAndKeys: //keys used in nib
			trig, @"key",
			[key hasPrefix:@"editbind"]?@"edit":@"", @"mode",
			[dict objectForKey:key], @"action",
			nil]];
	}
	return arr;
}


/*
 * extract a dictionary from the config files containing:
 * - name, team, gamma strings
 * - bind/editbind '.' key strings
 */
-(NSDictionary *)readConfigFiles 
{
	NSMutableDictionary *dict = [[NSMutableDictionary alloc] init];
	[dict setObject:@"" forKey:@"name"]; //ensure these entries are never nil
	[dict setObject:@"" forKey:@"team"]; 
		
	NSString *files[] = {@"config.cfg", @"autoexec.cfg"};
	int i;
	for(i = 0; i < sizeof(files)/sizeof(NSString*); i++) 
	{
		NSString *file = [[self cwd] stringByAppendingPathComponent:files[i]];
		NSArray *lines = [[NSString stringWithContentsOfFile:file] componentsSeparatedByString:@"\n"];

		if(i==0 && !lines)  // ugh - special case when first run...
		{ 
			file = [[self cwd] stringByAppendingPathComponent:@"data/defaults.cfg"];
			lines = [[NSString stringWithContentsOfFile:file] componentsSeparatedByString:@"\n"];
		}
		
		NSString *line; 
		NSEnumerator *e = [lines objectEnumerator];
		while(line = [e nextObject]) 
		{
			NSRange r; // more flexible to do this manually rather than via NSScanner...
			int j = 0;
			while(j < [line length] && [line characterAtIndex:j] <= ' ') j++; //skip white
			r.location = j;
			while(j < [line length] && [line characterAtIndex:j] > ' ') j++; //until white
			r.length = j - r.location;
			NSString *type = [line substringWithRange:r];
			
			while(j < [line length] && [line characterAtIndex:j] <= ' ') j++; //skip white
			if(j < [line length] && [line characterAtIndex:j] == '"') {
				r.location = ++j;
				while(j < [line length] && [line characterAtIndex:j] != '"') j++; //until close quote
				r.length = (j++) - r.location;
			} else {
				r.location = j;
				while(j < [line length] && [line characterAtIndex:j] > ' ') j++; //until white
				r.length = j - r.location;
			}
			NSString *value = [line substringWithRange:r];

			while(j < [line length] && [line characterAtIndex:j] <= ' ') j++; //skip white
			NSString *remainder = [line substringFromIndex:j];
			
			if([type isEqual:@"name"] || [type isEqual:@"team"] || [type isEqual:@"gamma"]) 
				[dict setObject:value forKey:type];
			else if([type isEqual:@"bind"] || [type isEqual:@"editbind"]) 
				[dict setObject:remainder forKey:[NSString stringWithFormat:@"%@.%@", type,value]];
		}
	}
	return dict;
}

-(void)updateAutoexecFile:(NSDictionary *)updates {
	NSString *file = [[self cwd] stringByAppendingPathComponent:@"autoexec.cfg"];
	//build the data 
	NSString *result = nil;
	NSArray *lines = [[NSString stringWithContentsOfFile:file] componentsSeparatedByString:@"\n"];
	if(lines) 
	{
		NSString *line; 
		NSEnumerator *e = [lines objectEnumerator];
		while(line = [e nextObject]) 
		{
			NSScanner *scanner = [NSScanner scannerWithString:line];
			NSString *type;
			if([scanner scanCharactersFromSet:[NSCharacterSet letterCharacterSet] intoString:&type])
				if([updates objectForKey:type]) continue; //skip things declared in updates
			result = (result) ? [NSString stringWithFormat:@"%@\n%@", result, line] : line;
		}
	}
	NSEnumerator *e = [updates keyEnumerator];
	NSString *type;
	while(type = [e nextObject]) 
	{
		id value = [updates objectForKey:type];
		if([type isEqual:@"name"] || [type isEqual:@"team"]) value = [NSString stringWithFormat:@"\"%@\"", value];
		NSString *line = [NSString stringWithFormat:@"%@ %@", type, value];
		result = (result) ? [NSString stringWithFormat:@"%@\n%@", result, line] : line;
	}
	//backup
	NSFileManager *fm = [NSFileManager defaultManager];
	NSString *backupfile = nil;
	if([fm fileExistsAtPath:file]) {
		backupfile = [file stringByAppendingString:@".bak"];
		if(![fm movePath:file toPath:backupfile handler:nil]) return; //can't create backup
	}	
	//write the new file
	if(![fm createFileAtPath:file contents:[result dataUsingEncoding:NSASCIIStringEncoding] attributes:nil]) return; //can't create new file		
	//remove the backup
	if(backupfile) [fm removeFileAtPath:backupfile handler:nil];
}



- (void)addResolutionsForDisplay:(CGDirectDisplayID)dspy 
{
    CFDictionaryRef mode;
    CFIndex i, cnt;
    CFArrayRef modeList = CGDisplayAvailableModes(dspy);
    
	if ( modeList == NULL )
		exit(1);
	
    cnt = CFArrayGetCount(modeList);
	
    for ( i = 0; i < cnt; ++i )
    {
        mode = CFArrayGetValueAtIndex(modeList, i);
        NSString *title = [NSString stringWithFormat:@"%i x %i", numberForKey(mode, kCGDisplayWidth), numberForKey(mode, kCGDisplayHeight)];
		if([resolutions itemWithTitle:title] == nil)
			[resolutions addItemWithTitle:title];
    }	
}

- (void)serverTerminated
{
	if(server==-1) return;
	server = -1;
	[multiplayer setTitle:@"Run"];
	[console appendText:@"\n \n"];
}

- (void)setServerActive:(BOOL)start
{
	if((server==-1) != start) return;
	
	if(!start)
	{	//STOP
		
		//damn server, terminate isn't good enough for you - die, die, die...
		if((server!=-1) && (server!=0)) kill(server, SIGKILL); //@WARNING - you do not want a 0 or -1 to be accidentally sent a  kill!
		[self serverTerminated];
	} 
	else
	{	//START
		NSString *cwd = [self cwd];
		NSArray *opts = [[serverOptions stringValue] componentsSeparatedByString:@" "];
		
		const char *childCwd  = [cwd fileSystemRepresentation];
		const char *childPath = [[cwd stringByAppendingPathComponent:@"sauerbraten.app/Contents/MacOS/sauerbraten"] fileSystemRepresentation];
		const char **args = (const char**)malloc(sizeof(char*)*([opts count] + 3)); //3 = {path, -d, NULL}
		int i, fdm, argc = 0;
		
		args[argc++] = childPath;
		args[argc++] = "-d";

		for(i = 0; i < [opts count]; i++)
		{
			NSString *opt = [opts objectAtIndex:i];
			if([opt length] == 0) continue; //skip empty
			args[argc++] = [opt UTF8String];
		}
		
		args[argc++] = NULL;

		switch ( (server = forkpty(&fdm, NULL, NULL, NULL)) ) // forkpty so we can reliably grab SDL console
		{ 
			case -1:
				[console appendLine:@"Error - can't launch server"];
				[self serverTerminated];
				break;
			case 0: // child
				chdir(childCwd);
				execv(childPath, (char*const*)args);
				fprintf(stderr, "Error - can't launch server\n");
				_exit(0);
			default: // parent
				free(args);
				//fprintf(stderr, "fdm=%d\n", slave_name, fdm);
				[multiplayer setTitle:@"Stop"];
				
				NSFileHandle *taskOutput = [[NSFileHandle alloc] initWithFileDescriptor:fdm];
				NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
				[nc addObserver:self selector:@selector(serverDataAvailable:) name:NSFileHandleReadCompletionNotification object:taskOutput];
				[taskOutput readInBackgroundAndNotify];
				break;
		}
	}
}

- (void)serverDataAvailable:(NSNotification *)note
{
	NSFileHandle *taskOutput = [note object];
    NSData *data = [[note userInfo] objectForKey:NSFileHandleNotificationDataItem];
	
    if (data && [data length])
	{
		NSString *text = [[NSString alloc] initWithData:data encoding:NSASCIIStringEncoding];		
		[console appendText:text];
		[text release];					
        [taskOutput readInBackgroundAndNotify]; //wait for more data
    }
	else
	{
		NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
		[nc removeObserver:self name:NSFileHandleReadCompletionNotification object:taskOutput];
		close([taskOutput fileDescriptor]);
		[self setServerActive:NO];
	}
}

/*
 * nil will just launch the fps game
 * "-rpg" will launch the rpg demo
 * otherwise we are specifying a map to play
 */
- (BOOL)playFile:(NSString*)filename
{
	NSArray *res = [[resolutions titleOfSelectedItem] componentsSeparatedByString:@" x "];	
	NSMutableArray *args = [[NSMutableArray alloc] init];
	NSString *cwd = [self cwd];
	
	//suppose could use this to update gamma and keys too, but can't be bothered...
	[self updateAutoexecFile:[NSDictionary dictionaryWithObjectsAndKeys:
		[name stringValue], @"name",
		[team stringValue], @"team",
		nil]];

	[args addObject:[NSString stringWithFormat:@"-w%@", [res objectAtIndex:0]]];
	[args addObject:[NSString stringWithFormat:@"-h%@", [res objectAtIndex:1]]];
	[args addObject:@"-z32"]; 
	
	if([fullscreen state] == NSOffState) [args addObject:@"-t"];
	[args addObject:[NSString stringWithFormat:@"-a%d", [fsaa intValue]]];
	[args addObject:[NSString stringWithFormat:@"-f%d", [shader intValue]]];
	
	if([filename isEqual:@"-rpg"])
		[args addObject:@"-grpg"]; //demo the rpg game
	else if(filename) 
		[args addObject:[NSString stringWithFormat:@"-l%@",filename]];
	
    if(![[advancedOptions stringValue] isEqual:@""])
        [args addObjectsFromArray:[[advancedOptions stringValue] componentsSeparatedByString:@" "]];
	
	NSTask *task = [[NSTask alloc] init];
	[task setCurrentDirectoryPath:cwd];
	[task setLaunchPath:[cwd stringByAppendingPathComponent:@"sauerbraten.app/Contents/MacOS/sauerbraten"]];
	[task setArguments:args];
	[task setEnvironment:[NSDictionary dictionaryWithObjectsAndKeys: @"1", @"SDL_ENABLEAPPEVENTS", nil]]; // makes Command-H, Command-M and Command-Q work at least when not in fullscreen
	[args release];
	
	BOOL okay = YES;
	
	NS_DURING
		[task launch];
		if(server==-1) [NSApp terminate:self];	//if there is a server then don't exit!
	NS_HANDLER
		//NSLog(@"%@", localException);
		NSRunAlertPanel(@"Error", @"Can't start Sauerbraten! Please move the directory containing Sauerbraten to a path that doesn't contain weird characters or start Sauerbraten manually.", @"OK", NULL, NULL);
		okay = NO;
	NS_ENDHANDLER
	
	return okay;
}

- (IBAction)multiplayerAction:(id)sender { [self setServerActive:(server==-1)]; }

- (IBAction)playAction:(id)sender { [self playFile:nil]; }

- (IBAction)playRpg:(id)sender { [self playFile:@"-rpg"]; }

- (IBAction)helpAction:(id)sender
{
	NSString *file = [[[[NSBundle mainBundle] bundlePath] stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"README.html"];
	NSWorkspace *ws = [NSWorkspace sharedWorkspace];
	[ws openURL:[NSURL fileURLWithPath:file]];
}

- (void)awakeFromNib
{
    CGDirectDisplayID display[kMaxDisplays];
    CGDisplayCount numDisplays;
    CGDisplayCount i;
    CGDisplayErr err;
	
    err = CGGetActiveDisplayList(kMaxDisplays, display, &numDisplays);
	
	if ( err != CGDisplayNoErr )
        exit(1);
    
	[resolutions removeAllItems];
    for ( i = 0; i < numDisplays; ++i )
        [self addResolutionsForDisplay:display[i]];
	[resolutions selectItemAtIndex: [[NSUserDefaults standardUserDefaults] integerForKey:@"resolution"]];
	
	NSDictionary *dict = [self readConfigFiles];
	
	[keys addObjects:[self getKeys:dict]];

	[name setStringValue:[dict objectForKey:@"name"]];
	[team setStringValue:[dict objectForKey:@"team"]];
	
	[serverOptions setFocusRingType:NSFocusRingTypeNone];
	[advancedOptions setFocusRingType:NSFocusRingTypeNone];
	
	server = -1;
	
	[[resolutions window] setDelegate:self]; // so can catch the window close
	
	[NSApp setDelegate:self]; //so can catch the double-click & dropped files
}

-(void)windowWillClose:(NSNotification *)notification
{
	[self setServerActive:NO];
	[NSApp terminate:self];
}

//we register 'ogz' as a doc type
- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename
{
	NSString *cwd = [[self cwd] stringByAppendingPathComponent:@"packages"];
	if(![filename hasPrefix:cwd])
	{
		if(NSRunAlertPanel(@"Can only load maps that are within the sauerbraten/packages/ folder.", @"Do you want to show this folder?", @"Ok", @"Cancel", nil))
			[[NSWorkspace sharedWorkspace] selectFile:cwd inFileViewerRootedAtPath:@""];
		//@TODO give user option to copy it into the packages folder?
		return NO;
	}
	filename = [filename substringFromIndex:[cwd length]+1]; //+1 to skip the leading '/'
	if([filename hasSuffix:@".ogz"]) filename = [filename substringToIndex:[filename length]-4]; //chop .ogz
	return [self playFile:filename];
}
@end
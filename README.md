>  [!CAUTION]  
> THIS IS NOT A VANGUARD BYPASS, if you try to start online matchmaking with this you will get kicked and punished, in some cases like Valorant your account will get banned.

# Vanguardless-Mode
Vanguard disabler for Valorant, 2XKO and League of Legends. Small tool to run these games without Vanguard for offline play or debugging purposes.

### Usage
Place vgc.exe in the same directory as launcher and run NoVgkLauncher.exe and select a game. This will automatically uninstall Vanguard but you can also uninstall it with Revo Uninstaller before running this app for complete removal. When your done using the app and want to go back to normal, simply open up cmd and enter these 2 commands: `sc stop vgc` then `sc delete vgc`. Next time you open Riot client youll be prompted to reinstall Vanguard so you can play online again.

### How it works
For League and 2XKO it hooks built in config flag to tell the game not to enforce Vanguard. In Valorant it spawns a fake vgc service that does nothing to make the game think Vanguard is running.

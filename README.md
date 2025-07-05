>  [!CAUTION]  
> THIS IS NOT A VANGUARD BYPASS, if you try to start online matchmaking (includes practice mode) with this, you will VAL 5 and in some cases penalized (temp ban).

# Vanguardless-Mode
A lightweight tool that lets you run Valorant without Vanguard. Meant for debugging safely (eg. sniffing with Fiddler/Charles Proxy, or dumping game binary). For League of Legends and 2XKO use [League Patch Collection](https://github.com/Cat1Bot/league-patch-collection) instead.

## Usage
1. Place FakeVgc.exe (a dummy/fake Vanguard executable) in the same directory as NoVgkLauncher.exe.
2. Run NoVgkLauncher.exe.
3. The launcher will automatically uninstall Vanguard (or you can uninstall manually using Revo Uninstaller) and start Valorant.
4. When you done and want to return to normal open up cmd as admin and enter `sc stop vgc` followed by `sc delete vgc`. Next time you open Riot client youll be prompted to reinstall Vanguard so you can play online again.

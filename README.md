# CS:GO External Hack
#### Features:
- Wall hack (Using glow)
  - Red - enemy with a ranged weapon (equipped)
  - Blue - enemy with an AWP (equipped)
  - Green - enemy with knife/grenade (equipped)
- Aim hack

#### How to use:
- Build the project (C++17 standard)
- Run CS:GO
- Run the .exe
- Press INSERT to toggle wallhack on/off
- Hold MIDDLE mouse button to activate aimlocking
- Press 'F3' to safely shutdown the process

#### About offsets
I will not be updating memory offsets often, you have to update them by yourself if something is not working.
Easiest way to achieve that is with 'hazedumer'

#### About the cheat
The entire idea of the cheat is educational (believe it or not).
It's a nice way to learn how to manipulate game memory and learn some math concepts in a 3D space (Damn you CalcAngle and Vectors...)
It was developed and tested purely AGAINST BOTS and never in competitive games.
Special thanks to laxodev for the memory manager done the right way -> https://github.com/laxodev/RAII-WINAPI-Memory-Manager

#### Current state
Currently the only optimized part of the cheat is the Memory Manager. Heavy refactoring on everything else is needed since there are lots of bad practises.

#### Detection status
Currently undetected by VAC but since I am releasing it publicly use at your own risk!

![alt text](https://i.imgur.com/2Xf5AFB.png)

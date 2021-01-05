# sss
Simple Shell Shooter

This is a lightweight and all-in-one synchronous singleplayer shooter that can be compiled by g++ and ran in a terminal. 
The game utilizes the ncurses library to display and update the map as rounds go on.

When any new map is loaded, SSS will first calculate all possible routes between every walk-able tile for the AI to use.
This uses the Dijkstra's shortest path algorithm, so the larger the map, the longer this will take. Once the map is loaded,
these paths will be stored in a file in the same directory, so any subsequent runs on the same map will not require
such calculations.

The key for any valid map is as follows:
 
	n - null, areas of the map that should not be accessable by players
	x - areas of the map that players cannot walk through
	o - obstacles that players cannot walk on, but can shoot through
	P - denotes a tile where the bomb can be planted  
	1 - denotes the general location of bomb site 1
	2 - denotes the general location of bomb site 2
	3 - denotes the general location of bomb site 3
	B - indicates spawn location of bomb
	T - indicates spawn location of T side players
	C - indicates spawn loaction of C side players
	
	A blank space is simply a tile that can be walked on.
	
See simplemap1.txt for an example of a valid map. An invalid map may result in pathfinding algorithm calculation errors,
which can either prevent the map from loading or result in unpredictable/incorrect AI behavior.
  
The style of the game is similar to tactical shooters like CS:GO. The winner is decided on the best of 3 rounds.
The @ symbol indicates the current location of the human player (if one exists).

When executing, there is one command line argument, which is the filename of the map that will be used.

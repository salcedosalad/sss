//#include<iostream>    //not used
#include <fstream>		//file input/output
#include <ncurses.h>	//oooooo ncurses library
#include <stdlib.h>		//to seed rand (srand)
#include <time.h>		//the thing to seed rand with (time)
#include <string>		//strings haha
#include <vector>		//i love vectors <3

void initCurses();	//for starting ncurses window, defined below
void endCurses();	//for ending ncurses window, defined below

//reads in map from file, map name, width, and height (functions defined below main)
class CharMap {
  public:
    CharMap(char *arg);
    CharMap(char** c, std::string m, int w, int h) : 
        map(c), mapName(m), width(w), height(h){}
    ~CharMap();
    void print();
    char ** map;	//holds map grid
    std::string mapName;
    int width;
    int height;
};

//represents an entity on the map (types of entities derive from this)
class ent_t {
public:
	//sets coordinates on creation
	ent_t(int xPos, int yPos) {	
		x = xPos;
		y = yPos;
	}
	
	//virtual destructor (for player_t)
	virtual ~ent_t() {}	
	
	//virtual function to return the identity of an entity
	virtual char whatamI() = 0;	
	
	//manually set coordinates
	void setCoordinates(int xPos, int yPos) {
		x = xPos;
		y = yPos;
	}
	
	//for debugging
	void printLocation() {
		printf("(%d, %d)", x, y); 
	}
	
	int x;	//x coordinate on map
	int y;	//y coordinate on map
};

//represents bomb (only one of these should exist at any time)
class bomb_t : public ent_t {
public:
	//initialize members
	bomb_t(int xPos, int yPos) : ent_t(xPos, yPos){
		isPlanted = false;
		isCarried = false;
		isDefused = false;
	}
	
	char whatamI() {
		return 'B';	//since its a bomb lol
	}
	
	//if a player picks it up, then its carried
	void pickedup() {
		isCarried = true;	
	}
	
	//if a player dies while holding the bomb, then die
	void dropped() {
		isCarried = false;	
	}
	
	bool isPlanted;	//flag for if the bomb is planted
	bool isCarried;	//flag for if a player is holding the bomb
	bool isDefused;	//flag for if a CT defuses
};

//represents a bullet that a player shoots
class projectile_t : public ent_t {
public:
	//initialize members
	projectile_t(int xPos, int yPos, char dir, ent_t* shooter) : ent_t(xPos, yPos){
		direction = dir;	//this should never change
		owner = shooter;	//this also should never change
	}
	
	char whatamI() {
		return '*';	//since its a bullet lol
	}
	
	char direction;	//direction that the bullet is traveling/shot in
	ent_t* owner;	//pointer to whatever shot a bullet
};

//represents a player (human or bot, T or CT)
class player_t : public ent_t {
public:
	//initialize members
	player_t(int xPos, int yPos, bool human, char side) : ent_t(xPos, yPos){
		xDest = yDest = xPlant = yPlant = -1;	//set initial destinations of AI
		tunnelCounter = 0;			//for AI tunnel handling
		killordefuse = rand() % 10;	//30% of CT's will choose to kill instead of defuse
		pathing = false;			//since AI don't path initially
		siteReached = false;		//since CT's dont spawn at a bombsite
		isAlive = true;
		isHuman = human;
		lastDirection = 'l';
		team = side;
		bomb = NULL;				//no one spawns carrying the bomb
	}
	
	//the only destructor out of the ent_t classes, since the player can be holding the bomb
	~player_t() {
		if (bomb != NULL)
			delete bomb;
	}
	
	char whatamI() {
		if (isHuman)
			return '@';		//since it's the human player lol
		else
			return team;	//either T or C
	}
	
	//if they pickup the bomb, make sure they reference it
	void pickupBomb(bomb_t* initBomb) {
		bomb = initBomb;
	}
	
	//for when a player dies
	void RIP(std::vector<ent_t*>& p_entList) {
		//if the bomb is held
		if (bomb != NULL) {		
			bomb->dropped();			//drop the bomb
			p_entList.push_back(bomb);	//make sure the bomb is on the map now
			bomb->setCoordinates(x, y);	//set the bomb's coordinates correctly
			bomb = NULL;
		}
		isAlive = false;	//dead lol
		
		//erase the player from its current entList
		for (int i = 0; i < p_entList.size(); i++) {
			if (p_entList[i] == this) 
				p_entList.erase(p_entList.begin() + i);
		}
	}

	bool isAlive;		//flag for aliveness
	bool isHuman;		//flag for humanness
	bomb_t* bomb;		//reference for bomb
	char lastDirection;	//tracks the last direction the player moved/shot in
	char team;			//T or C
	int xDest;			//x coordinate of AI destination
	int yDest;			//y coordinate of a CT's chosen bombsite
	int xPlant;			//x coordinate of AI destination
	int yPlant;			//y coordinate of a CT's destination
	int tunnelCounter;	//for tunnel handling
	int killordefuse;	//determines if a CT plays the actual objective lol
	bool siteReached;	//flag for if a CT has reached their chosen bombsite
	bool pathing;		//flag for if an AI has a destination
};

//represents a point on the map
class point_t {
public:
	//initialize members and setup the map (for Level)
	point_t(int xPos, int yPos, CharMap* grid) {
		x = xPos; 
		y = yPos;
		height = grid->height;
		total = height*(grid->width);
		isBombsite = false;
		isObstacle = false;
		isWall = false;
		isBridgeTunnel = false;
		parent = NULL;
		
		char here = grid->map[x][y];
		//if this spot on the map is not just a ' '
		if (here == 'x' || here == '#' || here == 'o' || here == 'n') {
			baseType = here;
			if (here == 'x')
				isWall = true;
			else if (here == '#')
				isBridgeTunnel = true;
			else if (here == 'o')
				isObstacle = true;
		}
		//if this spot on the map is walkable and/or has a specific purpose
		else if (here == ' ' || here == 'B' || here == 'P' || here == 'p' || here == '1' || here == '2' || here == '3') {
			baseType = ' ';
			//if its the bomb, spawn the bomb there
			if (here == 'B')
				entList.push_back(new bomb_t(x, y));
			//if its a plant site, set the flag to true
			else if (here == 'P' || here == 'p' || here == '1' || here == '2' || here == '3')
				isBombsite = true;
		}
		//if CT spawn, spawn 5 CT's
		else if (here == 'C') {
			baseType = ' ';
			for (int i = 0; i < 5; i++) {
				entList.push_back(new player_t(x, y, false, 'C'));
			}
		}
		//if T spawn, spawn 5 T's
		else if (here == 'T') {
			baseType = ' ';
			for (int i = 0; i < 5; i++) {
				entList.push_back(new player_t(x, y, false, 'T'));
			}
		}
	}
	
	//to empty out the entList and path array
	~point_t() {
		for (int i = 0; i < entList.size(); i++) {
			delete entList[i];
			entList[i] = NULL;
		}
		if (parent != NULL)
			delete [] parent;
	}
	
	//prints the point on the map based on what entities are there
	void renderPoint() {
		char finalType = baseType;
		bool hasBomb = false;
		bool hasProj = false;
		bool hasC = false;
		bool hasT = false;
		bool hasPlayer = false;
		
		if (isBombsite) finalType = 'P';
		
		//look through entList to find any entity we need to show
		for (int i = 0; i < entList.size(); i++) {
			if (entList[i]->whatamI() == '@') hasPlayer = true;
			if (entList[i]->whatamI() == 'T') hasT = true;
			if (entList[i]->whatamI() == 'C') hasC = true;
			if (entList[i]->whatamI() == '*') hasProj = true;
			if (entList[i]->whatamI() == 'B') hasBomb = true;
		}
		
		if (hasBomb) finalType = 'B';
		if (hasProj) finalType = '*';
		if (hasC) finalType = 'C';
		if (hasT) finalType = 'T';
		if (hasPlayer) finalType = '@';
		
		//importance (least to greatest): P, B, *, C, T, @
		printw("%c", finalType);
	}
	
	//removes a specific entity from the point's entList
	void deleteEntFromPoint(ent_t* e) {
		for (int i = 0; i < entList.size(); i++) {
			if (e == entList[i])
				entList.erase(entList.begin() + i);
		}
	}
	
	//figure out the next step in the shortest path from this point to another point
	char retrievePath(int xDest, int yDest) {
		int curr = height*yDest + xDest;
		
		//if already at destination, there's no need to move
		if (curr == height*y + x)
			return '!';
		
		//trace path through the parent arrays (like on the exam woooo)
		for (int i = 0; i < total; i++) {
			//stop at the point adjacent to the current point
			if (parent[curr] == height*y+x)	
				break;
			
			curr = parent[curr];
		}
		
		//move based on where the shortest path tells us to go
		if (curr == height*y+(x-1))
			return 'u';
		if (curr == height*y+(x+1))
			return 'd';
		if (curr == height*(y-1)+x)	
			return 'l';
		if (curr == height*(y+1)+x)	
			return 'r';
		else {
			//this shouldn't happen, but if it does then oops haha
			printw("Uh oh. C:(%d, %d) D:(%d, %d)", x, y, xDest, yDest);
			return '!';
		}
	}
	
	//respawn a team (for new rounds)
	void spawnTeam(char team) {
		for (int i = 0; i < 5; i++) {
			entList.push_back(new player_t(x, y, false, team));
		}
	}
	
	bool isBombsite;		//bombsiteness
	bool isObstacle;		//obstacleness
	bool isWall;			//wallness
	bool isBridgeTunnel;	//tunnelness
	char baseType;			//holds what kind of point on the map this is
	int x;					//x coordinate of point
	int y;					//x coordinate of point
	int height;				//height of map (for pathfinding)
	int total;				//total points on map (for pathfinding)
	std::vector<ent_t*> entList;	//contains all entities that may be on the point
	int* parent;			//shortest path info (from dijkstra's)
};

//represents the Level (complete with entities and game functionality and all that)
class Level {
public:
	//initialize members and allocate 2D point array
	Level(CharMap *maps) {
		mapref = maps;
		endCondition = 0;
		height = mapref->height;
		width = mapref->width;
		roundTimer = 300;
		bombTimer = 30;
		Talive = Calive = 5;
		Twins = Cwins = 0;
		bombPlanted = false;
		
		point = new point_t**[height];
		for (int i = 0; i < height; i++)
			point[i] = new point_t*[width];
		
		//create a new point at each coordinate
		for (int i = 0; i < height; i++) {
			for (int j = 0; j < width; j++) {
				point[i][j] = new point_t(i, j, mapref);
			}
		}
	}
	
	//for deallocating point array
	~Level() {
		if (point == NULL) return;
		for(int i = 0; i < height; i++) {
			for (int j = 0; j < width; j++) {
				delete point[i][j];
				point[i][j] = NULL;
			}
			delete [] point[i];
		}
		delete [] point;
		point = NULL;
	}
	
	//for printing the entire level (entities and all)
	void renderLevel() {
		for (int i = 0; i < height; i++) {
			for (int j = 0; j < width; j++) {
				point[i][j]->renderPoint();
			}
			printw("\n");
		}
	}
	
	//for preparing the screen for a new level render and printing timers
	void clearScreen() {
		clear();
		if (!bombPlanted)
			printw("%s: Round Time Left: %d\n", mapref->mapName.c_str(), roundTimer);
		else if (bombPlanted)
			printw("%s: Bomb Planted! Bomb Timer: %d\n", mapref->mapName.c_str(), bombTimer);
	}
	
	//returns pointer to a player object based on the human's team selection
	player_t* openNteamSelect() {
		printw("\n\n\nSelect your team!\n\n");
		printw("Press C for Counter-Terrorist team\nPress T for Terrorist team\nPress I for a bots-only match");
		printw("\n\nThe team who wins the best of 3 rounds wins the match!");
		char choice = getch();
		choice = toupper(choice);
		while (choice != 'C' && choice != 'T' && choice != 'I') {
			choice = getch();
			choice = toupper(choice);
		}
		clear();
		
		if (choice == 'C')
			printw("\n\n\nCounter-Terrorist team selected.\n\n");
		else if (choice == 'T')
			printw("\n\n\nTerrorist team selected.\n\n");
		else if (choice == 'I')
			printw("\n\n\nBot game selected.\n\n");
		
		printw("Press any key when you are ready to continue.");
		getch();
		clear();
		
		if (choice == 'I') {
			clear();
			return NULL;
		}
		
		//search for the player's spawn point (C or T)
		int x, y;
		for (int i = 0; i < height; i++) {
			for (int j = 0; j < width; j++) {
				if (mapref->map[i][j] == choice) {
					x = i;
					y = j;
				}
			}
		}
		
		//allocate player and spawn it at their team's spawnpoint
		player_t* player1 = new player_t(x, y, true, choice);
		point[x][y]->entList.push_back(player1);
		
		//delete a bot (since the human replaces it)
		delete (point[x][y]->entList[0]);
		point[x][y]->entList.erase(point[x][y]->entList.begin());
		
		clear();
		return player1;
	}
	
	//updates timer
	void secondTick() {
		roundTimer--;
	}
	
	//updates timer
	void bombTick() {
		bombTimer--;
	}
	
	//checks conditions for the end of a round and returns it (to main)
	int checkRoundStatus() {
		//this endcondition is set in MovementDispatcher
		if (endCondition == 1) {
			printw("The bomb has been defused. Counter-Terrorists win! ");
			Cwins++;
			return endCondition;
		}
		
		//if there is no more time and the bomb has not been planted
		if (roundTimer == 0 && !bombPlanted) {
			endCondition = 5;
			printw("The bomb has not been planted in time. Counter-Terrorists win! ");
			Cwins++;
		}
		//if T's all die without planting
		else if (Talive == 0 && !bombPlanted) {
			endCondition = 4;
			printw("All Terrorists have died before planting the bomb. Counter-Terrorists win! ");
			Cwins++;
		}
		//if CT's all die
		else if (Calive == 0) {
			endCondition = 3;
			printw("All Counter-Terrorists have died. Terrorists win! ");
			Twins++;
		}
		//if the bomb explodes
		else if (bombTimer == 0) {
			endCondition = 2;
			printw("The bomb has been detonated. Terrorists win! ");
			Twins++;
		}
		
		//decrement appropriate timers
		if (!bombPlanted)
			secondTick();
		else if (bombPlanted)
			bombTick();
		
		return endCondition;
	}
	
	//initialize a new round
	void newRound(bomb_t* bomb) {
		//reset different variables
		endCondition = 0;
		roundTimer = 300;
		bombTimer = 30;
		Talive = Calive = 5;
		bombPlanted = false;
		bomb->isPlanted = false;
		bomb->isCarried = false;
		bomb->isDefused = false;
		
		
		//clear the entire level of entities and respawn players/bomb
		for (int x = 0; x < height; x++) {
			for (int y = 0; y < width; y++) {
				while (point[x][y]->entList.size() > 0) {
					point[x][y]->entList.erase(point[x][y]->entList.begin());
				}
				
				if (mapref->map[x][y] == 'C')
					point[x][y]->spawnTeam('C');
				if (mapref->map[x][y] == 'T')
					point[x][y]->spawnTeam('T');
				if (mapref->map[x][y] == 'B') {
					point[bomb->x][bomb->y]->deleteEntFromPoint(bomb);
					point[x][y]->entList.push_back(bomb);
					bomb->setCoordinates(x, y);
				}
			}
		}
	}
	
	int endCondition;	//exactly what it says
	point_t*** point;	//2D array of pointers to points (crazy)
	int height;			//height of map
	int width;			//width of map
	CharMap* mapref;	//reference to original map
	int roundTimer;		//normal timer
	int bombTimer;		//timer if bomb planted
	bool bombPlanted;	//flag for bomb planted
	int Talive;			//tracks alive T's
	int Calive;			//tracks alive C's
	int Twins;			//tracks T wins
	int Cwins;			//tracks C wins
};

//handles bullet movement, hit detection, etc.
class BallisticDispatcher {
public:
	//initialize member
	BallisticDispatcher(Level* lvl) {
		levelref = lvl;
	}
	
	//adds a bullet to the list of bullets
	void addProjectile(projectile_t* bullet) {
		bulletList.push_back(bullet);
	}
	
	//deletes a bullet from the list of bullets, removes it from the point it is at, and deallocates
	void deleteProjectile(point_t* pt, projectile_t* bullet) {
		for (int i = 0; i < bulletList.size(); i++) {
			if (bulletList[i] == bullet)
				bulletList.erase(bulletList.begin() + i);
		}
		pt->deleteEntFromPoint(bullet);
		delete bullet;
	}
	
	//checks each bullet's status and make it move/kill a player 
	void updateAll() {
		int x, y, xMod, yMod;	//x and y coordinates
		char dir;		//direction the bullet is moving
		point_t* dest;	//reference to the point a bullet is at/moving to
		
		int i = 0;
		while (i < bulletList.size()) {
			//get bullet info
			x = bulletList[i]->x;
			y = bulletList[i]->y;
			dir = bulletList[i]->direction;
			
			//to modify x and y appropriately based on the direction
			if (dir == 'u') {
				xMod = -1;
				yMod = 0;
			}
			else if (dir == 'd') {
				xMod = 1;
				yMod = 0;
			}
			else if (dir == 'l') {
				xMod = 0;
				yMod = -1;
			}
			else if (dir == 'r') {
				xMod = 0;
				yMod = 1;
			}
			
			dest = levelref->point[x][y]; //first check where the bullet is
			
			int j = 0;
			bool killed = false;
			player_t* tempPlayer;
			
			//check if the bullet and a player (except the owner) are currently on the same spot
			while (j < dest->entList.size()) {
				if (dest->entList[j]->whatamI() == 'T' || dest->entList[j]->whatamI() == 'C' || dest->entList[j]->whatamI() == '@') {
					//if a non-owner player is found, kill it
					if (bulletList[i]->owner != dest->entList[j]) {
						tempPlayer = static_cast<player_t*>(dest->entList[j]);
						tempPlayer->RIP(dest->entList);
						//delete tempPlayer;
						killed = true;
						continue;
					}
				}
				j++;
			}
			
			dest = levelref->point[x+xMod][y+yMod]; //now check where the bullet is going
			
			//if it is going towards a wall, just despawn it
			if (dest->baseType == 'x') {
				deleteProjectile(levelref->point[x][y], bulletList[i]);
				continue;
			}
			
			//if it is not hitting a wall and theres no entities ahead, make it move
			if (dest->entList.size() == 0) {
				dest->entList.push_back(bulletList[i]);
				levelref->point[x][y]->deleteEntFromPoint(bulletList[i]);
				bulletList[i]->setCoordinates(x+xMod, y+yMod);
				i++;
				continue;
			}
			
			j = 0;
			killed = false;
			tempPlayer = NULL;
			
			//if there are entities, check if the bullet and a player (except the owner) will collide
			while (j < dest->entList.size()) {
				if (dest->entList[j]->whatamI() == 'T' || dest->entList[j]->whatamI() == 'C' || dest->entList[j]->whatamI() == '@') {
					//if a non-owner player is found, kill it
					if (bulletList[i]->owner != dest->entList[j]) {
						tempPlayer = static_cast<player_t*>(dest->entList[j]);
						tempPlayer->RIP(dest->entList);
						//delete tempPlayer;
						killed = true;
						continue;
					}
				}
				j++;
			}
			//delete the projectile if it collides, otherwise just move
			if (killed)
				deleteProjectile(levelref->point[x][y], bulletList[i]);
			else if (!killed) {
				dest->entList.push_back(bulletList[i]);
				levelref->point[x][y]->deleteEntFromPoint(bulletList[i]);
				bulletList[i]->setCoordinates(x+xMod, y+yMod);
				i++;
				continue;
			}
		}
	}

	std::vector<projectile_t*> bulletList;	//list of all bullets on the map
	Level* levelref;	//reference to the level object
};

//handles player/bot movement and shooting
class MovementDispatcher {
public:
	//retrieves input from the user on the next command to perform
	static char readkeyinput() {
		int input = getch(); //this is an int so it can read arrow keys
		char result = '!'; //initially set to '!' (in case they enter garbage)
		
		//check input and set result accordingly
		if (input == KEY_UP || input == 'W' || input == 'w')
			result = 'u';
		if (input == KEY_DOWN || input == 'S' || input == 's')
			result = 'd';
		if (input == KEY_LEFT || input == 'A' || input == 'a')
			result = 'l';
		if (input == KEY_RIGHT || input == 'D' || input == 'd')
			result = 'r';
		if (input == 'I' || input == 'i')
			result = 'i';
		if (input == 'C' || input == 'c')
			result = 'c';
		if (input == 'Q' || input == 'q')
			result = 'q';
		if (input == ' ')
			result = ' ';
		
		//if the result is still !, recursively call this function
		if (result == '!')
			return readkeyinput();
		
		return result;
	}
	
	//handles whatever action a player/bot needs to perform
	static void makeMove(Level* lvl, player_t* p, char command, BallisticDispatcher* bullets) {
		if (p == NULL)
			return;
		if (!(p->isAlive))
			return;
		
		int x = p->x;
		int y = p->y;
		int xMod, yMod;
		
		//to modify x and y accordingly based on direction
		if (command == 'u') {
			xMod = -1;
			yMod = 0;
		}
		else if (command == 'd') {
			xMod = 1;
			yMod = 0;
		}
		else if (command == 'l') {
			xMod = 0;
			yMod = -1;
		}
		else if (command == 'r') {
			xMod = 0;
			yMod = 1;
		}
		
		//if the command is to move
		if (command == 'u' || command == 'd' || command == 'l' || command == 'r') {
			//if they are not running into a wall/obstacle
			if (lvl->point[x+xMod][y+yMod]->baseType != 'o' && lvl->point[x+xMod][y+yMod]->baseType != 'x') {
				//if they are not in a tunnel, move normally
				if (lvl->point[x][y]->baseType != '#') {
					lvl->point[x+xMod][y+yMod]->entList.push_back(p);
					lvl->point[x][y]->deleteEntFromPoint(p);
					p->setCoordinates(x+xMod, y+yMod);
					p->lastDirection = command;
				}
				//if they are in a tunnel, only let them move parallel to the direction they entered in
				else if ((command == 'u' || command == 'd') && p->lastDirection != 'l' && p->lastDirection != 'r') {
					lvl->point[x+xMod][y+yMod]->entList.push_back(p);
					lvl->point[x][y]->deleteEntFromPoint(p);
					p->setCoordinates(x+xMod, y+yMod);
					p->lastDirection = command;
				}
				//if they are in a tunnel, only let them move parallel to the direction they entered in
				else if ((command == 'l' || command == 'r') && p->lastDirection != 'u' && p->lastDirection != 'd') {
					lvl->point[x+xMod][y+yMod]->entList.push_back(p);
					lvl->point[x][y]->deleteEntFromPoint(p);
					p->setCoordinates(x+xMod, y+yMod);
					p->lastDirection = command;
				}
			}
		}
		
		//clear the screen if c
		if (command == 'c')
			clear();
		
		//shoot a bullet if space
		if (command == ' ') {
			//create new bullet, add it to the current point/bullet list
			projectile_t* newBullet = new projectile_t(x, y, p->lastDirection, p);
			lvl->point[x][y]->entList.push_back(newBullet);
			bullets->addProjectile(newBullet);
			newBullet = NULL; //not necessary but why not be safe i guess
		}
		
		//perform necessary checks after movement is done
		postMovementChecks(lvl, p, bullets);
	}
	
	//checks states of player/bots to see if certain things can be done
	static void postMovementChecks(Level *lvl, player_t *p, BallisticDispatcher* bullets) {
		//get player coordinates
		int x = p->x;
		int y = p->y;
		
		/* debugging check for bot positioning
		if (p->isHuman == false)
			printw("(%d,%d) ", p->x, p->y);
		*/

		//if a terrorist walks over the bomb, pick it up
		if (p->team == 'T' && p->bomb == NULL) {
			for (int i = 0; i < lvl->point[x][y]->entList.size(); i++) {
				if (lvl->point[x][y]->entList[i]->whatamI() == 'B') {
					p->pickupBomb(static_cast<bomb_t*>(lvl->point[x][y]->entList[i]));
					p->bomb->pickedup();
					lvl->point[x][y]->deleteEntFromPoint(p->bomb);
					break;
				}
			}
		}
		//if a terrorist has the bomb and walks onto a bombsite, plant it
		if (p->team == 'T' && p->bomb != NULL && lvl->point[x][y]->isBombsite) {
			p->bomb->isPlanted = true;
			p->bomb->isCarried = false;
			p->bomb->setCoordinates(x, y);
			lvl->point[x][y]->entList.push_back(p->bomb);
			p->bomb = NULL;
			lvl->bombPlanted = true;
			printw("Bomb has been planted!");
		}
		//if a counter-terrorist walks over a planted bomb, defuse it
		if (p->team == 'C' && lvl->bombPlanted) {
			for (int i = 0; i < lvl->point[x][y]->entList.size(); i++) {
				if (lvl->point[x][y]->entList[i]->whatamI() == 'B') {
					lvl->endCondition = 1;
					break;
				}
			}
		}
	}
};

//handles the adjacency matrix of the map and the dijkstra's algorithm for pathfinding
class adjMatrix {
public:
	//initialize members and create adjacency matrix
	adjMatrix(Level* lvl) {
		levelref = lvl;
		height = levelref->height;
		width = levelref->width;
		total = height * width;	//the matrix should be (h*w) x (h*w)
		
		matrix.resize(total);
		for (int i = 0; i < total; i++)
			matrix[i].resize(total);
		
		for (int i = 0; i < total; i++) {
			for (int j = 0; j < total; j++) {
				if (i != j)
					matrix[i][j] = 0;
				else if (i == j)
					matrix[i][j] = 1;
			}
		}
		
		int xMod, yMod;	//for checking up, down, above, and below
		
		//check every point on the map and make connections to any adjacent spaces (that can be walked on)
		for (int x = 0; x < height; x++) {
			for (int y = 0; y < width; y++) {
				if (levelref->point[x][y]->baseType == ' ' || levelref->point[x][y]->baseType == '#') {
					//check point above
					xMod = x-1; yMod = y;
					if (levelref->point[xMod][yMod]->baseType == ' ' || levelref->point[x][y]->baseType == '#') {
						matrix[height*y+x][height*yMod+xMod] = 1;
						matrix[height*yMod+xMod][height*y+x] = 1;
					}
					//check point below
					xMod = x+1; yMod = y;
					if (levelref->point[xMod][yMod]->baseType == ' ' || levelref->point[x][y]->baseType == '#') {
						matrix[height*y+x][height*yMod+xMod] = 1;
						matrix[height*yMod+xMod][height*y+x] = 1;
					}
					//check point left
					xMod = x; yMod = y-1;
					if (levelref->point[xMod][yMod]->baseType == ' ' || levelref->point[x][y]->baseType == '#') {
						matrix[height*y+x][height*yMod+xMod] = 1;
						matrix[height*yMod+xMod][height*y+x] = 1;
					}
					//check point right
					xMod = x; yMod = y+1;
					if (levelref->point[xMod][yMod]->baseType == ' ' || levelref->point[x][y]->baseType == '#') {
						matrix[height*y+x][height*yMod+xMod] = 1;
						matrix[height*yMod+xMod][height*y+x] = 1;
					}
				}
			}
		}
	}
	
	//finally we are at dijkstra's, which is what this assignment was for in the first place
	//calculates path from one point to every other point on the map (so it takes a while)
	void dijkstra(point_t* p) {
		if (total == 0) //if theres no map (shouldn't happen) don't do anything
			return;
		
		int inf = 2147483647;		//represents infinity
		int x = p->x;				//get player x and y coords
		int y = p->y;
		int pIndex = height*y+x;	//get corresponding index in adj matrix
		int src; 					//holds index with minimum key
		int min;
		
		int key[total];			//array of weights for each vertex
		bool visited[total];	//tracks whether a vertex has been visited already
		//the parent array is stored inside the point that given when this is called
		
		key[pIndex] = 0; //first set the starting point's key to 0 to select it first
		
		//set all other keys to infinity, mark parents as -1, and start visited as false
		//i had to initialize parents or else outputting paths to a file would not work
		for (int i = 0; i < total; i++) {
			if (i != pIndex) {
				key[i] = inf;
			}
			p->parent[i] = -1;
			visited[i] = false;
		}
		
		//dijkstra's algorithm
		for (int i = 0; i < total; i++) {
			//start src and min at inf to find the minimum
			src = inf; 
			min = inf;
			
			//find vertex with smallest key
			for (int j = 0; j < total; j++) {
				if (key[j] < min && visited[j] == false) {
					min = key[j];
					src = j;
				}
			}
			
			//this shouldn't happen, but in case there is an unconnected section of the map
			//this will allow the algorithm to start again at any unconnected part
			if (src == inf || min == inf) {
				for (int x = 0; x < total; x++) {
					if (key[x] == min && visited[x] == false) {
						src = x;
						break;
					}
				}
			}
			
			//for every other vertex
			for (int dest = 0; dest < total; dest++) {
				//if there is an edge from the current vertex to any unvisited vertex whose sum with the current key
				//is smaller than that vertex's key, then update that vertex's key and parent
				if (src != dest && matrix[src][dest] != 0 && (key[src]+matrix[src][dest] < key[dest]) && !visited[dest]) {
					key[dest] = key[src]+matrix[src][dest];
					p->parent[dest] = src;
				}
			}
			
			//set current vertex as visited
			visited[src] = true;
		}
		
		//the parent array of the point provided in the arguments
		//should now be used to figure out the shortest path from that point
		//to any other point on the map. this is also written to a map's path file.
	}
	
private:
	std::vector<std::vector<int>> matrix;	//adjacency matrix (imagine using a list for this lmao)
	Level* levelref;	//reference to level object
	int total;			//total amount of points (vertices) in map
	int height;			//height of map
	int width;			//width of map
};

//handles AI pathing and behavior woooooooo
class AIDispatcher {
public:
	//sets reference to human
	void addHuman(player_t* player) {
		human = player;
	}
	
	//adds bot reference to list of bots
	void addBot(player_t* bot) {
		botList.push_back(bot);
	}
	
	//sets reference to bomb
	void addBomb(bomb_t* boom) {
		bomb = boom;
	}
	
	//initialize members and load/calculate shortest paths for AI (and write to file if necessary)
	AIDispatcher(Level* lvl, BallisticDispatcher* bullets, adjMatrix* graph, std::fstream &inPath) {
		levelref = lvl;
		bulletref = bullets;
		graphref = graph;
		bomb = NULL;
		human = NULL;
		totalBots = 0;	//i never actually increment or use this, but its okay
		
		char ch = '!';	//i had to initialize it to something or else i get an invalid read
		clear();
		
		//ask player if they want to load an existing path file or run dijkstra's and generate one
		printw("\n\n\nPress Y to load paths from this map's paths file or N to calculate them.");
		printw("\n\nThe map's path file should be named: %s", (levelref->mapref->mapName + "_paths.txt").c_str());
		printw("\n\nAny path calculations will be written to a .txt file named after the map.");
		printw("\nThis way, future runs with this map can load paths from that .txt file.");
		while (ch != 'Y' && ch != 'N' && ch != 'y' && ch != 'n') {
			ch = getch();
		}
		
		//if they choose to load a file, open the file for input
		if (ch == 'Y' || ch == 'y')
			inPath.open((levelref->mapref->mapName + "_paths.txt").c_str(), std::fstream::in);
		//if they choose to run dijkstra's, open the file for output (overwrite any existing data)
		else
			inPath.open((levelref->mapref->mapName + "_paths.txt").c_str(), std::fstream::out | std::fstream::trunc);
		
		//if the user tries to load the file and it doesn't exist, just start running dijkstra's to make one
		if (!inPath.is_open()) {
			clear();
			printw("\n\n\n%s could not be found. Calculating and writing path file instead.", (levelref->mapref->mapName + "_paths.txt").c_str());
			printw("\n\nPress any key to continue.\n");
			getch();
			ch = 'n';
			inPath.open((levelref->mapref->mapName + "_paths.txt").c_str(), std::fstream::out | std::fstream::trunc);
		}
		
		//for every single point on the map
		int count = 0;
		for (int x = 0; x < levelref->height; x++) {
			//print current status of loading/calculating progress
			clear();
			if (ch == 'Y' || ch == 'y') {
				printw("\n\n\nLoading shortest paths from %s for map row %d out of %d...", (levelref->mapref->mapName + "_paths.txt").c_str(), x+1, levelref->height);
				printw("\n\nTotal Dijkstra calculations loaded: %d", count);
			}
			else {
				printw("\n\n\nCalculating shortest paths for map row %d out of %d...", x+1, levelref->height);
				printw("\n\nTotal Dijkstra calculations: %d", count);
			}
			refresh();
			
			for (int y = 0; y < levelref->width; y++) {
				//whenever we hit a point that can be walked on
				if (levelref->point[x][y]->baseType == ' ') {
					//allocate parent array for shortest path
					levelref->point[x][y]->parent = new int[levelref->height*levelref->width];
					
					//if they load from a file, then load from that file
					if (ch == 'Y' || ch == 'y') {
						for (int i = 0; i < levelref->height*levelref->width; i++) {
							inPath >> levelref->point[x][y]->parent[i];
						}
					}
					//otherwise, run dijkstra's on the point and write the results to a file
					else {
						graph->dijkstra(levelref->point[x][y]);
						for (int i = 0; i < levelref->height*levelref->width; i++) {
							inPath << levelref->point[x][y]->parent[i] << " ";
						}
						inPath << "\n";
					}
					
					count++; //to count how many points have been loaded/calculated
				}
				
				//add references to bomb/bot/human whenever we come across them
				for (int i = 0; i < levelref->point[x][y]->entList.size(); i++) {
					if (levelref->point[x][y]->entList[i]->whatamI() == 'B')
						addBomb(static_cast<bomb_t*>(levelref->point[x][y]->entList[i]));
					else if (levelref->point[x][y]->entList[i]->whatamI() == 'T')
						addBot(static_cast<player_t*>(levelref->point[x][y]->entList[i]));
					else if (levelref->point[x][y]->entList[i]->whatamI() == 'C')
						addBot(static_cast<player_t*>(levelref->point[x][y]->entList[i]));
					else if (levelref->point[x][y]->entList[i]->whatamI() == '@')
						addHuman(static_cast<player_t*>(levelref->point[x][y]->entList[i]));
				}
			}
		}
		
		inPath.close();
		
		clear();
		printw("\n\n\nDone loading! Press any key to continue.");
		if (ch == 'Y' || ch == 'y')
			printw("\n\nTotal Dijkstra calculations loaded: %d", count);
		else {
			printw("\n\nTotal Dijkstra calculations: %d", count);
			printw("\n\nShortest paths have been written to %s.", (levelref->mapref->mapName + "_paths.txt").c_str());
		}
		getch();
	}
	
	//checks status of all players to see if anything died and needs to be deallocated
	void checkForNewDead() {
		int i = 0;
		//if a bot dies, indicate it in the level object and delete it
		while (i < botList.size()) {
			if (!(botList[i]->isAlive)) {
				if (botList[i]->team == 'T')
					(levelref->Talive)--;
				else if (botList[i]->team == 'C')
					(levelref->Calive)--;
				delete botList[i];
				botList.erase(botList.begin() + i);
				continue;
			}
			i++;
		}
		//if the human dies, indicate it in level and delete it
		if (human != NULL && !(human->isAlive)) {
			if (human->team == 'T')
				(levelref->Talive)--;
			else if (human->team == 'C')
				(levelref->Calive)--;
			delete human;
			human = NULL;
		}
	}
	
	//checks the line of sight of a bot to see if they can shoot an enemy
	void checkView(player_t* bot) {
		char dir = bot->lastDirection;
		char side = bot->team;
		int x = bot->x; 
		int y = bot->y;
		bool enemyFound = false;
		bool checkOpp = false;
		bool finishCheck = false;
		point_t* currPoint;
		
		//checks ahead of the bot and to the sides (if they saw behind them thats dumb)
		//it is prioritized so that the bot will shoot whatever is in front of them
		//before shooting whatever is to the sides
		while (true) {
			while (true) {
				//keep going through each point that they can see (based on direction)
				if (dir == 'u') x--;
				if (dir == 'd') x++;
				if (dir == 'l') y--;
				if (dir == 'r') y++;
				currPoint = levelref->point[x][y];
				
				//if their "vision" hits a wall, then stop looking
				if (currPoint->baseType == 'x')
					break;
				//check the entList of the point that is being looked at
				for (int i = 0; i < currPoint->entList.size(); i++) {
					//if a player is there that is not the owner, then mark that an enemy is found
					if (currPoint->entList[i]->whatamI() == 'T' || currPoint->entList[i]->whatamI() == 'C' || currPoint->entList[i]->whatamI() == '@') {
						if (static_cast<player_t*>(currPoint->entList[i])->team != side) {
							enemyFound = true;
							break;
						}
					}
				}
				if (enemyFound)
					break;
			}
			//if an enemy was found or we've finished checking all 3 lines of sight, then stop
			if (enemyFound || finishCheck)
				break;
			
			//now, check the opposite of whatever we just checked
			if (checkOpp) {
				//reset coordinates to the bot's position
				x = bot->x; 
				y = bot->y;
				if (dir == 'u') dir = 'd';
				if (dir == 'd') dir = 'u';
				if (dir == 'l') dir = 'r';
				if (dir == 'r') dir = 'l';
				finishCheck = true;	//the next check is the last one
				continue;
			}
			
			//mark that we need to check the opposite of whatever side we check next
			checkOpp = true;
			//reset coordinates to the bot's position
			x = bot->x; 
			y = bot->y;
			//switch direction 90 degrees
			if (dir == 'u') dir = 'l';
			if (dir == 'l') dir = 'd';
			if (dir == 'd') dir = 'r';
			if (dir == 'r') dir = 'u';
		}
		
		//if an enemy was found during any of the checks, then shoot (the bot should look in that direction first)
		if (enemyFound) {
			bot->lastDirection = dir;
			MovementDispatcher::makeMove(levelref, bot, ' ', bulletref);
		}
	}
	
	//checks the line of sight (of a CT) to see if an enemy is visible
	//see checkView for comments on this function (its the same thing, just without shooting)
	bool detectEnemy(player_t* bot, int &xEnemy, int &yEnemy) {
		
		char dir = bot->lastDirection;
		char side = bot->team;
		int x = bot->x; 
		int y = bot->y;
		bool enemyFound = false;
		bool finishCheck = false;
		bool checkOpp = false;
		point_t* currPoint;
		
		while (true) {
			while (true) {
				if (dir == 'u') x--;
				if (dir == 'd') x++;
				if (dir == 'l') y--;
				if (dir == 'r') y++;
				currPoint = levelref->point[x][y];
				
				if (currPoint->baseType == 'x')
					break;
				for (int i = 0; i < currPoint->entList.size(); i++) {
					if (currPoint->entList[i]->whatamI() == 'T' || currPoint->entList[i]->whatamI() == 'C' || currPoint->entList[i]->whatamI() == '@') {
						if (static_cast<player_t*>(currPoint->entList[i])->team != side) {
							enemyFound = true;
							xEnemy = currPoint->entList[i]->x;
							yEnemy = currPoint->entList[i]->y;
							break;
						}
					}
				}
				if (enemyFound)
					break;
			}
			if (enemyFound || finishCheck)
				break;
			
			if (checkOpp) {
				x = bot->x; 
				y = bot->y;
				if (dir == 'u') dir = 'd';
				if (dir == 'd') dir = 'u';
				if (dir == 'l') dir = 'r';
				if (dir == 'r') dir = 'l';
				finishCheck = true;
				continue;
			}
			
			checkOpp = true;
			x = bot->x; 
			y = bot->y;
			if (dir == 'u') dir = 'l';
			if (dir == 'l') dir = 'd';
			if (dir == 'd') dir = 'r';
			if (dir == 'r') dir = 'u';
		}
		
		if (enemyFound)
			return true;
		else
			return false;
	}
	
	//determines a bot's destination based on the state of the round
	char checkPath(player_t* bot) {
		int x, y;
		
		//if they are in a tunnel, then make sure they exit safely
		if (levelref->point[bot->x][bot->y]->baseType == '#') {
			if (bot->tunnelCounter == 0)
				bot->tunnelCounter = 15;
		}
		if (bot->tunnelCounter > 0) {
			bot->tunnelCounter--;
			return bot->lastDirection;
		}
		
		//bots won't move for first 5 ticks of the round
		if (levelref->roundTimer >= 295) {
			return '!';
		}
		
		//if the destination is reached, set pathing to false and reset destination
		if (bot->xDest == bot->x && bot->yDest == bot->y) {
			bot->pathing = false;
			bot->xDest = -1;
			bot->yDest = -1;
			//the first destination reached by a CT should be the bombsite
			if (bot->team == 'C') {
				bot->siteReached = true;
			}
		}
		
		//there is a 10% chance for a bot to move in a random direction
		int randMove = rand() % 10;
		if (randMove < 1) {
			int randDir = rand() % 4;
			if (randDir == 0) return 'u';
			if (randDir == 1) return 'd';
			if (randDir == 2) return 'l';
			if (randDir == 3) return 'r';
		}
		
		//for terrorist bots
		if (bot->team == 'T') {
			//if the bot is carrying the bomb
			if (bot->bomb != NULL) {
				//if they aren't already pathing, pick a random site to path to
				if (!(bot->pathing)) {
					while (true) {
						x = rand() % levelref->height;
						y = rand() % levelref->width;
						
						if (levelref->point[x][y]->isBombsite) {
							bot->pathing = true;
							bot->xDest = x;
							bot->yDest = y;
							break;
						}
					}
				}
			}
			//if bomb is not planted/carried, find the bomb
			else if (!(levelref->bombPlanted) && !(bomb->isCarried)) {
				bot->pathing = true;
				bot->xDest = bomb->x;
				bot->yDest = bomb->y;
			}
			//if bomb is not planted but the human is carrying the bomb, follow human
			else if (!(levelref->bombPlanted) && human != NULL && human->bomb != NULL) {
				bot->pathing = true;
				bot->xDest = human->x;
				bot->yDest = human->y;
			}
			//if bomb is not planted and neither the human or this bot is carrying the bomb, follow the bomb carrier
			else if (!(levelref->bombPlanted) && bot->bomb == NULL) {
				for (int i = 0; i < botList.size(); i++) {
					if (botList[i]->bomb != NULL) {
						bot->pathing = true;
						bot->xDest = botList[i]->x;
						bot->yDest = botList[i]->y;
						break;
					}
				}
			}
			//if the bomb is planted
			else if (levelref->bombPlanted) {
				//if they aren't already pathing to a spot near the bomb, start pathing to a random spot near the bomb
				if (!(bot->pathing) || !(bot->xDest-bomb->x > -10 && bot->xDest-bomb->x < 10 && bot->yDest-bomb->y > -10 && bot->yDest-bomb->y < 10)) {
					while (true) {
						x = rand() % levelref->height;
						y = rand() % levelref->width;
						
						if ((x-(bomb->x) > -10 && x-(bomb->x) < 10) && (y-(bomb->y) > -10 && y-(bomb->y) < 10)) {
							if (levelref->point[x][y]->baseType == ' ') {
								bot->pathing = true;
								bot->xDest = x;
								bot->yDest = y;
								break;
							}
						}
					}
				}
			}
		}
		
		//for counter-terrorist bots
		if (bot->team == 'C') {
			int enemyDetected = false;
			int xEnemy, yEnemy;
			
			//check every bot's line of sight to see if an enemy has been seen
			for (int i = 0; i < botList.size(); i++) {
				if (botList[i]->team == 'C') {
					enemyDetected = detectEnemy(botList[i], xEnemy, yEnemy);
					if (enemyDetected) {
						bot->siteReached = false;
						break;
					}
				}
			}
			
			//if bomb is not planted, terrorist has not been seen, and they haven't reached a site, then go to a random site
			if (!(levelref->bombPlanted) && !enemyDetected && !(bot->siteReached)) {
				if (!(bot->pathing)) {
					while (true) {
						x = rand() % levelref->height;
						y = rand() % levelref->width;
						
						if (levelref->point[x][y]->isBombsite) {
							bot->pathing = true;
							bot->xPlant = x;
							bot->yPlant = y;
							bot->xDest = x;
							bot->yDest = y;
							break;
						}
					}
				}
			}
			//if bomb is not planted, no enemy has been seen, but they are indeed at a site
			else if (!(levelref->bombPlanted) && !enemyDetected && bot->siteReached) {
				//if they are at a site, stay near the site that has been chosen
				if (!(bot->pathing) || !(bot->xDest-bot->xPlant > -10 && bot->xDest-bot->xPlant < 10 && bot->yDest-bot->yPlant > -10 && bot->yDest-bot->yPlant < 10)) {
					while (true) {
						x = rand() % levelref->height;
						y = rand() % levelref->width;
						
						if ((x-(bot->xPlant) > -10 && x-(bot->xPlant) < 10) && (y-(bot->yPlant) > -10 && y-(bot->yPlant) < 10)) {
							if (levelref->point[x][y]->baseType == ' ') {
								bot->pathing = true;
								bot->xDest = x;
								bot->yDest = y;
								break;
							}
						}
					}
				}
			}
			//if bomb is planted and a terrorist has been seen, then rotate 
			//the round timer check is so that a rotation doesn't happen too early or else they're trolling
			else if (!(levelref->bombPlanted) && levelref->roundTimer < 250 && enemyDetected) {
				bot->pathing = true;
				bot->xDest = xEnemy;
				bot->yDest = yEnemy;
			}
			//if the bomb is planted, go to defuse (normally)
			else if (levelref->bombPlanted) {
				//there is a 30% chance that a counter-terrorist bot will want to go for kills
				if (bot->killordefuse < 3) {
					bot->pathing = true;
					bot->xDest = xEnemy;
					bot->yDest = yEnemy;
				}
				bot->pathing = true;
				bot->xDest = bomb->x;
				bot->yDest = bomb->y;
			}
		}
		
		//retrieve the direction to move to using the shortest path info stored in the current point
		return levelref->point[bot->x][bot->y]->retrievePath(bot->xDest, bot->yDest);
	}
	
	//updates the status of all bots each tick
	void updateAll() {
		//check if anything died
		checkForNewDead();
		
		//runs for every existing bot
		for (int i = 0; i < botList.size(); i++) {
			//see if a bot can shoot
			checkView(botList[i]);
			
			//get the direction that a bot needs to move in (a '!' means don't move)
			char moveDir = checkPath(botList[i]);
			//printw("%c ", moveDir); //for debugging
			//getch(); //for debugging
			
			if (moveDir == '!')
				continue;
			
			//move accordingly
			MovementDispatcher::makeMove(levelref, botList[i], moveDir, bulletref);
		}
	}
	
	//for initializing a new round
	player_t* newRound() {
		//delete any bots, the human (if they exist), and any bullets
		while (botList.size() > 0) {
			levelref->point[botList[0]->x][botList[0]->y]->deleteEntFromPoint(botList[0]);
			delete botList[0];
			botList.erase(botList.begin());
			continue;
		}
		if (human != NULL) {
			levelref->point[human->x][human->y]->deleteEntFromPoint(human);
			delete human;
			human = NULL;
		}
		while (bulletref->bulletList.size() > 0) {
			bulletref->deleteProjectile(levelref->point[bulletref->bulletList[0]->x][bulletref->bulletList[0]->y], bulletref->bulletList[0]);
		}
		
		//tell the level to start a new round and have the human select a team
		levelref->newRound(bomb);
		human = levelref->openNteamSelect();
		
		//run through all the points again to add references to the bomb/bot/human
		for (int x = 0; x < levelref->height; x++) {
			for (int y = 0; y < levelref->width; y++) {
				for (int i = 0; i < levelref->point[x][y]->entList.size(); i++) {
					if (levelref->point[x][y]->entList[i]->whatamI() == 'B')
						addBomb(static_cast<bomb_t*>(levelref->point[x][y]->entList[i]));
					else if (levelref->point[x][y]->entList[i]->whatamI() == 'T')
						addBot(static_cast<player_t*>(levelref->point[x][y]->entList[i]));
					else if (levelref->point[x][y]->entList[i]->whatamI() == 'C')
						addBot(static_cast<player_t*>(levelref->point[x][y]->entList[i]));
					else if (levelref->point[x][y]->entList[i]->whatamI() == '@')
						addHuman(static_cast<player_t*>(levelref->point[x][y]->entList[i]));
				}
			}
		}
		
		//return pointer to the human player to main
		return human;
	}
	
	std::vector<player_t*> botList;	//list of bots
	player_t* human;		//reference to human
	bomb_t* bomb;			//reference to bomb
	adjMatrix* graphref;	//reference to adjMatrix for dijkstra
	Level* levelref;		//reference to level
	int totalBots;			//not used but its here lol
	BallisticDispatcher* bulletref;	//reference to bullet handler
};

//so many lines...
int main(int argc, char **argv) {
	srand(time(NULL)); //seed rand for cool AI rng haha
	
    CharMap *map;
	if (argc == 2)
		map = new CharMap(argv[1]);
	else
		map = NULL;
	
	//close if no file given
    if (map == NULL) {
		printf("\nInvalid map file!\n\n"); 
		return 0;
	} 
	
    initCurses(); //initialize ncurses window and stuff
	
	//welcome message, instructions, warnings
	printw("\n\n\nWelcome to Counter Strike: Valorant Offensive\nPress any key to begin your journey!\n");
	printw("\n\nINSTRUCTIONS:\nQ\t\tQuit\nARROWS\t\tMove\nWASD\t\tMove\nSpace\t\tShoot\nI\t\tIdle\nC\t\tClear screen\n");
	printw("\n\nWARNINGS:\nLevel will not display correctly if the terminal is too small for the map.\n");
	printw("AI will not function properly if paths are loaded from the wrong .txt file.\n");
	printw("The program may crash or break if path files are modified in any way.");
	getch();
	clear();
	
	std::fstream inPath; //for reading/writing paths to a file
	Level* lvl = new Level(map); //generate level that entities will play on
	player_t* player1 = lvl->openNteamSelect(); //create human player and pick a team
	BallisticDispatcher* ballistics = new BallisticDispatcher(lvl); //start bullet handling
	adjMatrix graph(lvl); //create adjacency matrix of the map
	AIDispatcher* AI = new AIDispatcher(lvl, ballistics, &graph, inPath); //start bot/AI handling
	
	inPath.close(); //close file if it was still open for some reason
	
	char ch; //holds user input
	bool keepPlaying = true; //flag to keep playing rounds until an exit condition is reached
	while (keepPlaying) {
		//prompt user to start round
		clear();
		printw("\n\n\nPress any key to start the round!");
		getch();
		clear();
		
		lvl->clearScreen();	//clear screen for new output
		lvl->renderLevel(); //render the level
		
		//keep receiving user input until the round ends (or the user ragequits)
		while ((ch = MovementDispatcher::readkeyinput()) != 'q' && ch != 'Q') {
			//only allow player action if they're still alive
			if (player1 != NULL) 
				MovementDispatcher::makeMove(lvl, player1, ch, ballistics);
			
			AI->updateAll();			//update AI in the game
			ballistics->updateAll();	//update bullets in the game
			lvl->clearScreen();			//clear screen for new output
			lvl->renderLevel();			//rerender the level
			
			if (lvl->checkRoundStatus() != 0) {break;} //exit this loop if the round ends
		}
		//tell the user they quit if they quit
		if (ch == 'q' || ch == 'Q') {
			printw("You have quit the round (hopefully not out of rage). ");
		}
		ch = 'n';
		printw("\nPress C to continue.\n");
		while (ch != 'C' && ch != 'c') {
			ch = getch();
		}
		clear();
		//keep playing rounds until one team wins the best of 3 (or the user wants to stop)
		if (lvl->Cwins < 2 && lvl->Twins < 2) {
			printw("\n\n\nScore: C %d-%d T", lvl->Cwins, lvl->Twins);
			printw("\n\nPress Y to play another round or Q to quit the game.\n");
		}
		else if (lvl->Cwins >= 2 || lvl->Twins >= 2) {
			printw("\n\n\nFinal score: C %d-%d T", lvl->Cwins, lvl->Twins);
			//print appropriate victory message
			if (lvl->Cwins > lvl->Twins )
				printw("\n\nCounter-Terrorists win the match! Press any key to quit the game.\n");
			else
				printw("\n\nTerrorists win the match! Press any key to quit the game.\n");
			getch();
			ch = 'Q'; //so that the rounds stop when a team wins
		}		
		while (ch != 'Y' && ch != 'Q' && ch != 'y' && ch != 'q') {
			ch = getch();
		}
		
		//exit if they want to quit (or if a team has won the match)
		if (ch == 'Q' || ch == 'q')
			break;
		//otherwise, start up a new round
		else {
			clear();
			player1 = AI->newRound();
			clear();
		}
	}
    delete map; map = NULL;
	delete lvl; lvl = NULL;
	delete ballistics; ballistics = NULL;
	delete AI; AI = NULL;
	clear();
	//for politeness
    printw("\n\n\nThank you for playing!");
    endCurses(); //END CURSES
    return 0;
}

void initCurses(){
    //curses initializations
	initscr();
	raw();
	keypad(stdscr, TRUE);
	noecho();
}

void endCurses(){
	refresh();
	getch(); //make the user press any key to close the game
	endwin();
}

//CharMap functions are defined below
CharMap::CharMap(char *arg){
    char temp;
    std::ifstream fin(arg); //to read in map from file
	//if the file could not be open/found, then say so
	if (fin.is_open() == false) {
		printf("\nCould not find/open map file.\n\n");
		exit(0);
	}
    fin >> mapName; //first thing in map file is name
    fin >> height;  //next is height
    fin >> temp;	//then the 'x'
    fin >> width;	//and then width
	
	//allocate the 2D array for the map
    map = new char*[height]; 
    for(int i=0; i<height; i++){
        map[i] = new char[width];
		//read in each character on the map to its appropriate spot in the array
        for(int j=0; j<width; j++) 
            fin >> (map[i][j]) >> std::noskipws; //dont skip whitespace
        fin >> std::skipws; //skip whitespace (should just be newline)
    }
}

CharMap::~CharMap(){
    if(map == NULL) return;		//if there's no map, then don't do anything (shouldn't happen)
		
	//deallocate the 2D map array
    for(int i=0; i<height; i++)
        delete [] map[i];
    delete [] map;
}

//since it uses printw, don't call this until initCurses() is called
void CharMap::print(){ 
    printw("Read Map: '%s' with dimensions %dx%d!\n", 
            mapName.c_str(), height, width);
	
	//print the map
    for(int i=0; i<height; i++){
        for(int j=0; j<width; j++) {
            printw("%c", map[i][j]);
			refresh();
		}
        printw("\n");
    }   
}
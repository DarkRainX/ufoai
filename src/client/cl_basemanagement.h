#ifndef BASEMANGEMENT_DEFINED
#define BASEMANGEMENT_DEFINED 1

#define	BSFS(x)	(int)&(((building_t *)0)->x)

#define	PRODFS(x)	(int)&(((production_t *)0)->x)

#define REPAIRTIME 14
#define REPAIRCOSTS 10

#define UPGRADECOSTS 100
#define UPGRADETIME  2

#define MAX_PRODUCTIONS		256

#define MAX_LIST_CHAR		1024
#define MAX_BUILDINGS		256
#define MAX_BASES		8
#define MAX_DESC		256

#define BUILDINGCONDITION	100

#define SCROLLSPEED		1000

// this is not best - but better than hardcoded every time i used it
#define RELEVANT_X		187.0
#define RELEVANT_Y		280.0
// take the values from scriptfile
#define BASEMAP_SIZE_X		778.0
#define BASEMAP_SIZE_Y		672.0

#define BASE_SIZE		5
#define MAX_BASE_LEVELS		1

// max values for employee-management
#define MAX_EMPLOYEES 1024
#define MAX_EMPLOYEES_IN_BUILDING 64
// TODO:
// MAX_EMPLOYEES_IN_BUILDING should be redefined by a config variable that is lab/workshop/quarters-specific
// e.g.:
// if ( !maxEmployeesInQuarter ) maxEmployeesInQuarter = MAX_EMPLOYEES_IN_BUILDING;
// if ( !maxEmployeesWorkersInLab ) maxEmployeesWorkersInLab = MAX_EMPLOYEES_IN_BUILDING;
// if ( !maxEmployeesInWorkshop ) maxEmployeesInWorkshop = MAX_EMPLOYEES_IN_BUILDING;

// allocate memory for menuText[TEXT_STANDARD] contained the information about a building
char	buildingText[MAX_LIST_CHAR];

// The types of employees
typedef enum
{
	EMPL_UNDEF,
	EMPL_SOLDIER,
	EMPL_SCIENTIST,
	EMPL_WORKER,		// unused right now
	EMPL_MEDIC,		// unused right now
	EMPL_ROBOT,		// unused right now
	MAX_EMPL			// for counting over all available enums
} employeeType_t;

// The definition of an employee
typedef struct employee_s
{
	employeeType_t type;		// What profession does this employee has.

	char speed;			// Speed of this Worker/Scientist at research/construction.

	struct building_s *quarters;	// The quarter this employee is assigned to. (all except EMPL_ROBOT)
	struct building_s *lab;		// The lab this scientist is working in. (only EMPL_SCIENTIST)
	struct building_s *workshop;	// The lab this worker is working in. (only EMPL_WORKER)
	//struct building_s *sickbay;	// The sickbay this employee is medicaly treated in. (all except EMPL_ROBOT ... since all can get injured.)
	//struct building_s *training_room;	// The room this soldier is training in in. (only EMPL_SOLDIER)

	struct character_s *combat_stats;	// Soldier stats (scis/workers/etc... as well ... e.g. if the base is attacked)
} employee_t;


// Struct to be used in building definition - List of employees.
typedef struct employees_s
{
	struct employee_s *assigned[MAX_EMPLOYEES_IN_BUILDING];	// List of employees.
	int numEmployees;								// Current number of employees.
	int maxEmployees;								// Max. number of employees (from config file)
	float cost_per_employee;							// Costs per employee that are added to toom-total-costs-
} employees_t;

typedef enum
{
	BASE_NOT_USED,
	BASE_UNDER_ATTACK,
	BASE_WORKING
} baseStatus_t;

typedef enum
{
	B_NOT_SET, // not build yet
	B_UNDER_CONSTRUCTION, // right now under construction
	B_CONSTRUCTION_FINISHED, // construction finished - but no workers assigned
				 // and building needs workers
	B_UPGRADE, // just upgrading (not yet implemented)
	B_WORKING_120, // a quick boost (not yet implemented)
	B_WORKING_100, // working normal (or workers assigned when needed)
	B_WORKING_50, // damaged or something like that
	B_MAINTENANCE, // yes, maintenance - what else to say
	B_REPAIRING, // repairing to get back status B_WORKING_100
	B_DOWN // totally damaged
} buildingStatus_t;

typedef enum
{
	B_MISC,
	B_LAB,
	B_QUARTERS,
	B_WORKSHOP,
	B_HANGAR
} buildingType_t;

typedef struct building_s
{
	char	name[MAX_VAR];
	char	title[MAX_VAR];
	char	text[MAX_VAR]; // short description
	// needs determines the second building part
	char	*image, *needs, *depends, *mapPart, *produceType, *pedia;
	float	energy, workerCosts, produceTime, fixCosts, varCosts;
	int	production, level, id, timeStart, buildTime, techLevel, notUpOn;
	int	base; // number of base this building is located in.

	int	maxWorkers, minWorkers, assignedWorkers; // TODO: remove these and replace them with the employee_s struct
	// A list of employees assigned to this building.
	struct employees_s assigned_employees;

	//if we can build more than one building of the same type:
	buildingStatus_t	buildingStatus[BASE_SIZE*BASE_SIZE];
	int	condition[BASE_SIZE*BASE_SIZE];

	vec2_t	size;
	byte	visible;
	// needed for baseassemble
	// when there are two tiles (like hangar) - we only load the first tile
	int	used;

	// event handler functions
	char	onConstruct[MAX_VAR];
	char	onAttack[MAX_VAR];
	char	onDestroy[MAX_VAR];
	char	onUpgrade[MAX_VAR];
	char	onRepair[MAX_VAR];
	char	onClick[MAX_VAR];

	//more than one building of the same type allowed?
	int	moreThanOne;

	//how many buildings are there of the same type?
	//depends on the value of moreThanOne ^^
	int	howManyOfThisType;

	//position of autobuild
	vec2_t	pos;

	//autobuild when base is set up
	byte	autobuild;

	//autobuild when base is set up
	byte	firstbase;

	//this way we can rename the buildings without loosing the control
	buildingType_t	buildingType;

	struct building_s *dependsBuilding;
} building_t;

#define MAX_AIRCRAFT	256
#define MAX_CRAFTUPGRADES	64
#define LINE_MAXSEG 64
#define LINE_MAXPTS (LINE_MAXSEG+2)
#define LINE_DPHI	(M_PI/LINE_MAXSEG)

typedef struct mapline_s
{
	int    n;
	float  dist;
	vec2_t p[LINE_MAXPTS];
} mapline_t;

typedef enum
{
	AIRCRAFT_TRANSPORTER,
	AIRCRAFT_INTERCEPTOR,
	AIRCRAFT_UFO
} aircraftType_t;

typedef enum
{
	CRAFTUPGRADE_WEAPON,
	CRAFTUPGRADE_ENGINE,
	CRAFTUPGRADE_ARMOR
} craftupgrade_type_t;

typedef struct craftupgrade_s
{
	/* some of this informations defined here overwrite the ones in the aircraft_t struct if given. */

	/* general */
	char    id[MAX_VAR];		// Short (unique) id/name.
	char    name[MAX_VAR];		// Full name of this upgrade
	craftupgrade_type_t type;	// weapon/engine/armor
	technology_t* pedia;		// -pedia link

	/* armor related */
	float armor_kinetic;		// maybe using (k)Newtons here?
	float armor_shield;			// maybe using (k)Newtons here?

	/* weapon related */
	struct craftupgrade_t *ammo;
	struct weapontype_t *weapontype;	// e.g beam/particle/missle ( do we already have something like that?)
	int num_ammo;
	float    damage_kinetic;		// maybe using (k)Newtons here?
	float    damage_shield;		// maybe using (k)Newtons here?
	float    range;				// meters (only for non-beam weapons. i.e projectiles, missles, etc...)

	/* drive related */
	int    speed;				// the maximum speed the craft can fly with this upgrade
	int    fuelSize;				// the maximum fuel size

	/* representation/display */
	char    model[MAX_QPATH];	// 3D model
	char    image[MAX_VAR];		// image
} craftupgrade_t;

typedef struct aircraft_s
{
	char	name[MAX_VAR];	// translateable name
	char	title[MAX_VAR];	// internal id
	char	image[MAX_VAR];	// image on geoscape
	aircraftType_t	type;
	int		status;	// see aircraftStatus_t
	float		speed;
	int	price;
	int	fuel;	// actual fuel
	int	fuelSize;	// max fuel
	int	size;	// how many soldiers max
	vec2_t	pos;	// actual pos on geoscape
	int		point;
	int		time;
	int	teamSize;	// how many soldiers on board
	// TODO
	// xxx teamShape;    // TODO: shape of the soldier-area onboard.
	char	model[MAX_QPATH];
	char	weapon_string[MAX_VAR];
	technology_t*	weapon;
	char	shield_string[MAX_VAR];
	technology_t*	shield;
	mapline_t route;
	void*	homebase;	// pointer to homebase

	char    building[MAX_VAR];	// id of the building needed as hangar

	craftupgrade_t    *upgrades[MAX_CRAFTUPGRADES];	// TODO replace armor/weapon/engine definitions from above with this.
	int    numUpgrades;
	struct aircraft_s *next; // just for linking purposes - not needed in general
} aircraft_t;

typedef struct base_s
{
	//the internal base-id
	int	id;
	char	title[MAX_VAR];
	int	map[BASE_SIZE][BASE_SIZE][MAX_BASE_LEVELS];

	qboolean founded;
	vec2_t pos;

	// to decide which actions are available in the basemenu
	byte	hasHangar;
	byte	hasLab;

	//this is here to allocate the needed memory for the buildinglist
	char	allBuildingsList[MAX_LIST_CHAR];

	//mapChar indicated which map to load (gras, desert, arctic,...)
	//d=desert, a=arctic, g=gras
	char	mapChar;

	// all aircraft in this base
	// FIXME: make me a linked list (see cl_market.c aircraft selling)
	aircraft_t	aircraft[MAX_AIRCRAFT];
	int 	numAircraftInBase;
	void*	aircraftCurrent;

	int	posX[BASE_SIZE][BASE_SIZE][MAX_BASE_LEVELS];
	int	posY[BASE_SIZE][BASE_SIZE][MAX_BASE_LEVELS];

	//FIXME: change building condition to base condition
	float	condition;

	baseStatus_t	baseStatus;

	// which level to display?
	int	baseLevel;

	int		hiredMask;
	int		teamMask;
	int		deathMask;

	int		numHired;
	int		numOnTeam;
	int		numWholeTeam;

	// the onconstruct value of the buliding
	// building_radar increases the sensor width
	int	sensorWidth; // radar radius

	inventory_t	teamInv[MAX_WHOLETEAM];
	inventory_t	equipment;

	character_t	wholeTeam[MAX_WHOLETEAM];
	character_t	curTeam[MAX_ACTIVETEAM];
	character_t	*curChr;

	int		equipType;
	int		nextUCN;

	// needed if there is another buildingpart to build
	struct building_s *buildingToBuild;

	struct building_s *buildingCurrent;
} base_t;

extern	base_t	bmBases[MAX_BASES];					// A global list of _all_ bases.
extern	base_t	*baseCurrent;							// Currently displayed/accessed base.
extern	building_t	bmBuildings[MAX_BASES][MAX_BUILDINGS];	// A global list of _all_ buildings (even unbuilt) in all bases.
extern	int numBuildings;								// The global number of entries in the bmBuildings list.

extern	employee_t	employees[MAX_EMPLOYEES];			// This it the global list of employees.
extern	int   numEmployees;							// The global number of entries in the "employees" list.

craftupgrade_t    *craftupgrades[MAX_CRAFTUPGRADES];		// This it the global list of all available craft-upgrades
int    numCraftUpgrades;								// The global number of entries in the "craftupgrades" list.

void MN_InitEmployees ( void );
void CL_UpdateBaseData( void );
base_t* B_GetBase ( int id );
void B_UpgradeBuilding( building_t* b );
void B_RepairBuilding( building_t* b );
int B_CheckBuildingConstruction ( building_t* b );
int B_GetNumOnTeam ( void );
building_t * MN_GetFreeBuilding( int base_id, buildingType_t type );
building_t * MN_GetUnusedLab( int base_id );
int MN_GetUnusedLabs( int base_id );
void MN_ClearBuilding( building_t *building );
int MN_EmployeesInBase2 ( int base_id, employeeType_t employee_type, byte free_only );
byte MN_RemoveEmployee ( building_t *building );
byte MN_AssignEmployee ( building_t *building_dest, employeeType_t employee_type );
void B_SaveBases( sizebuf_t *sb );
void B_LoadBases( sizebuf_t *sb, int version );
void MN_ParseBuildings( char *title, char **text );
void MN_ParseBases( char *title, char **text );
void MN_BuildingInit( void );
void B_AssembleMap( void );
void B_BaseAttack ( void );
void MN_DrawBuilding( void );
building_t* B_GetBuildingByID ( int id );
building_t* B_GetBuilding ( char *buildingName );

typedef struct production_s
{
	char    name[MAX_VAR];
	char    title[MAX_VAR];
	char    text[MAX_VAR]; //short description
	int	amount;
	char	menu[MAX_VAR];
} production_t;

extern vec2_t newBasePos;

void MN_BuildingInit( void );
int B_GetCount ( void );
void B_SetUpBase ( void );

void MN_ParseProductions( char *title, char **text );
void MN_SetBuildingByClick ( int row, int col );
void MN_ResetBaseManagement( void );
void MN_ClearBase( base_t *base );
void MN_NewBases( void );

#endif /* BASEMANGEMENT_DEFINED */


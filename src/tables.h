// Weapon and native-cheat tables shared by the menu and the hotkey list.
#pragma once

struct WeaponDef { int type; const char* name; int ammo; };
struct WeaponGroup { const char* group; WeaponDef w[8]; };
inline const WeaponGroup kWeapons[] = {
    { "Melee",    { {1,"Brass Knuckles",1},{2,"Screwdriver",1},{3,"Golf Club",1},{4,"Nightstick",1},
                    {5,"Knife",1},{6,"Baseball Bat",1},{7,"Hammer",1},{8,"Meat Cleaver",1} } },
    { "Blades",   { {9,"Machete",1},{10,"Katana",1},{11,"Chainsaw",1} } },
    { "Thrown",   { {12,"Grenades",30},{13,"Remote Grenades",30},{14,"Tear Gas",30},{15,"Molotovs",30} } },
    { "Pistols",  { {17,"Colt .45",300},{18,"Python",300} } },
    { "Shotguns", { {19,"Shotgun",200},{20,"SPAS-12",200},{21,"Stubby Shotgun",200} } },
    { "SMGs",     { {22,"Tec-9",500},{23,"Uzi",500},{24,"Silenced Ingram",500},{25,"MP5",500} } },
    { "Rifles",   { {26,"M4",500},{27,"Ruger",500},{28,"Sniper Rifle",100},{29,"Laser Sniper",100} } },
    { "Heavy",    { {30,"Rocket Launcher",50},{31,"Flamethrower",1000},{32,"M60",1000},{33,"Minigun",1000} } },
    { "Other",    { {36,"Camera",100} } },
};

// Codes decoded from CPad::AddToPCCheatString in this exe; running one feeds it
// to the game exactly as if it had been typed.
struct NativeCheat { const char* code; const char* desc; };
struct CheatGroup { const char* group; NativeCheat c[12]; };
inline const CheatGroup kCheats[] = {
    { "Player", { {"PRECIOUSPROTECTION","Full armour"},{"ASPIRINE","Full health"},
                  {"YOUWONTTAKEMEALIVE","Wanted level +2"},{"LEAVEMEALONE","Clear wanted level"},
                  {"ICANTTAKEITANYMORE","Suicide"},{"CERTAINDEATH","Smoke a cigar"},
                  {"FANNYMAGNET","Women follow you"},{"STILLLIKEDRESSINGUP","Change outfit"} } },
    { "Weapons", { {"THUGSTOOLS","Weapon set 1"},{"PROFESSIONALTOOLS","Weapon set 2"},
                   {"NUTTERTOOLS","Weapon set 3"} } },
    { "Weather & time", { {"APLEASANTDAY","Sunny weather"},{"ALOVELYDAY","Very sunny weather"},
                          {"ABITDRIEG","Overcast weather"},{"CATSANDDOGS","Rain"},
                          {"CANTSEEATHING","Fog"},{"LIFEISPASSINGMEBY","Fast clock"},
                          {"ONSPEED","Faster gameplay"},{"BOOOOOORING","Slower gameplay"} } },
    { "Vehicle spawns", { {"PANZER","Rhino tank"},{"TRAVELINSTYLE","Bloodring Banger"},
                          {"GETTHEREQUICKLY","Bloodring Banger (2)"},{"GETTHEREFAST","Sabre Turbo"},
                          {"GETTHEREVERYFASTINDEED","Hotring Racer"},{"GETTHEREAMAZINGLYFAST","Hotring Racer (2)"},
                          {"THELASTRIDE","Romero's Hearse"},{"ROCKANDROLLCAR","Love Fist limo"},
                          {"RUBBISHCAR","Trashmaster"},{"BETTERTHANWALKING","Caddy"} } },
    { "Vehicles", { {"BIGBANG","Blow up nearby cars"},{"GRIPISEVERYTHING","Better handling"},
                    {"COMEFLYWITHME","Flying cars"},{"SEAWAYS","Cars drive on water"},
                    {"AIRSHIP","Flying boats"},{"WHEELSAREALLINEED","Only wheels visible"},
                    {"LOADSOFLITTLETHINGS","Bigger wheels"},{"GREENLIGHT","All traffic lights green"},
                    {"MIAMITRAFFIC","Aggressive traffic"},{"AHAIRDRESSERSCAR","Pink traffic"},
                    {"IWANTITPAINTEDBLACK","Black traffic"} } },
    { "Pedestrians", { {"FIGHTFIGHTFIGHT","Peds riot"},{"NOBODYLIKESME","Peds attack you"},
                       {"OURGODGIVENRIGHTTOBEARARMS","Peds carry weapons"},{"CHICKSWITHGUNS","Armed women"},
                       {"HOPINGIRL","Peds get in your car"},{"CHASESTAT","Media level meter"},
                       {"DEEPFRIEDMARSBARS","Fat player"},{"PROGRAMMER","Skinny player"} } },
    { "Skins", { {"LOOKLIKELANCE","Lance Vance"},{"MYSONISALAWYER","Ken Rosenberg"},
                 {"ILOOKLIKEHILARY","Hilary King"},{"ROCKANDROLLMAN","Love Fist (Jezz)"},
                 {"WELOVEOURDICK","Love Fist (Dick)"},{"ONEARMEDBANDIT","Phil Cassidy"},
                 {"IDONTHAVETHEMONEYSONNY","Sonny Forelli"},{"FOXYLITTLETHING","Mercedes"},
                 {"CHEATSHAVEBEENCRACKED","Ricardo Diaz"},{"IWANTBIGTITS","Candy Suxxx"} } },
};

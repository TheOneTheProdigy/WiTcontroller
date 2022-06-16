const String appName = "witController";
const String appVersion = "Version 0.1";
const String msg_start = "Start";

const String msg_browsing_for_service ="Browsing for WiT services";

const String menu_menu = "#: Key Defs       *: Menu";
const String menu_finish = "#: Finish";
const String menu_cancel = "*: Cancel";
const String menu_show_direct = "#: Show Direct";
const String menu_roster =  "0-9: Select *: Cancel";
const String menu_turnout_list =  "0-9: Select *: Cancel";
const String menu_route_list =  "0-9: Select *: Cancel";

const String menu_select_wit_service = "0-9: Select    Enc_Btn: Sleep";
const String menu_select_ssids = "0-9: Select      Enc_Btn: Sleep";

const String direct_command_list = "Direct Commands";

const String direction_forward = "         Fwd";
const String direction_reverse = "         Rev";

const String msg_no_ssids_found = "No ssids found";
const String msg_ssids_found = " ssids found";
const String msg_bounjour_setup_failed = "Unable to setup Listener";
const String msg_no_services_found = "No services found";
const String msg_services_found = " Service(s) found";
const String msg_trying_to_connect = "Trying to Connect";
const String msg_connected = "Connected";
const String msg_address_label = "IP address: ";
const String msg_connection_failed = "Connection failed";
const String msg_disconnected = "Disconnected";
const String msg_start_sleep = "Starting Sleep  Enc_Btn: Wake";
const String msg_no_loco_selected = "No Loco selected";

// const String label_locos = "Locos: ";
// const String label_speed = "Speed: ";
const String label_track_power = "TRK";
// const String label_unknown = "?";
// const String label_on = "On" ;
// const String label_off = "Off";

#define KEYPAD_USE_OPERATION 0
#define KEYPAD_USE_SELECT_SSID 1
#define KEYPAD_USE_SELECT_WITHROTTLE_SERVER 2
#define KEYPAD_USE_SELECT_ROSTER 3
#define KEYPAD_USE_SELECT_TURNOUTS_THROW 4
#define KEYPAD_USE_SELECT_TURNOUTS_CLOSE 5
#define KEYPAD_USE_SELECT_ROUTES 6

// used for both wit and ssid
#define CONNECTION_STATE_DISCONNECTED 0
#define CONNECTION_STATE_CONNECTED 1
#define CONNECTION_STATE_SELECTION_REQUIRED 2
#define CONNECTION_STATE_SELECTED 3

#define MAX_LOCOS     10  // maximum number of locos that can be added to the consist

const String menuText[10][2] = {
  {"Function",    "no+#: Select        *: Cancel"},                //0
  {"Add Loco",    "no+#: Add  #: Roster *: Cancel"},        //1
  {"Drop Loco",   "no+#: One  #: All    *: Cancel"}, //2
  {"Toggle Dir",  ""},                               //3
  {"",""},                                          //4
  {"Throw Point", "no+#: Throw  #: List *: Cancel"},       //5
  {"Close Point", "no+#: Close  #: List *: Cancel"},      //6
  {"Route",       "no+#: Select #: List *: Cancel"},            //7
  {"Trk Power",   "#: Toggle           *: Cancel"},                  //8 
  {"Disconnect",  "9+#: Sleep      #: Disconnect"},             //9
};


// speed increase for each click of the encoder 
const int speedStep = 4;
const int speedStepMultiplier = 2;  // for 'fast' speed steps

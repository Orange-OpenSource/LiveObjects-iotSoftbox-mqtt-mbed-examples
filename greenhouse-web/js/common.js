/**
*
* VALUES AND FUNCTIONS SHARED BETWEEN THE APPLICATIONS' MODULES
*
*/
'use strict';
// Here, set your own API Key
var API_KEY = "XXX"; 
// Here, set your device id (MAC address)
//var ASSET_ID = "2e:47:52:93:d1:88"; // Aurélien
var ASSET_ID = "a6:43:fc:f6:28:f4"; //Jérôme
var SERVER_ADDR = 'liveobjects.orange-business.com'
var ASSET_NAMESPACE = "ghsensor";
var STREAM_ID = "urn:lo:nsid:"+ ASSET_NAMESPACE + ":" + ASSET_ID + "!measures";
var STREAM_ID_LAMP ="urn:lo:nsid:"+ ASSET_NAMESPACE + ":" + ASSET_ID + "!leds";
var STREAM_ID_ALERT ="urn:lo:nsid:"+ ASSET_NAMESPACE + ":" + ASSET_ID + "!states";
var current_light = 0;
var luminosity_collector = [];
//Id of the matching rule
var rule1_id = 0;
var rule2_id = 0;
var rule1_active = false;
var rule2_active = false;
var value_save = false;
var rule1_operator = "<";
var rule1_mailto;
var rule1_threshold = "-1";
var rule1_lightnumber ="0";
var rule1_action;
var rule2_operator = "<";
var rule2_mailto;
var rule2_threshold = "-1";
var rule2_lightnumber = "0";
var rule2_action;
var SEND_MAIL = '{\
 "destination": \
  {\
    "to": "{0}",\
    "cc": "",\
    "cci": ""\
  },\
  "message": {\
     "type": "application/text",\
     "subject": "Alert",\
     "content": "Luminosity sensor is no longer available"\
  }\
}'
var SWITCH_ON_LIGHT = '{\
  "event": "LIGHT-ON",\
  "data": {\
    "number" : {0}\
  }\
}';
var UPDATE_RULE_MAIL='{\
  "id": "{0}",\
  "name": "{1}",\
  "enabled": {2},\
  "dataPredicate": {\
    "==": [\
      {\
        "var": "value.LightSensorState"\
      },\
      0\
    ]\
  }\
}'
var UPDATE_RULE_THRESHOLD='{\
  "id": "{0}",\
  "name": "{1}",\
  "enabled": {2},\
  "dataPredicate": {\
    "{3}": [\
      {\
        "var": "value.LightSensor"\
      },\
      {4}\
    ]\
  }\
}';
function print_luminosity_from_now(desired_minute){
  //console.log("Normal print")
  current_window = desired_minute;
  current_start_date = new Date();
  // We compute the end date we want for the measures
  current_end_date = new Date(current_start_date.getTime()-desired_minute*60*1000);
  print_luminosity(current_start_date, current_end_date)
}
function light_request(nb_lights){
  $.ajax({
    url: "https://" + SERVER_ADDR + "/api/v0/assets/"+ ASSET_NAMESPACE + "/" + ASSET_ID + "/commands",
    beforeSend: function(xhr) {
    },
    type: 'POST',
    headers:{
      "Accept": "application/json",
      "X-API-KEY": API_KEY
    },
    dataType: 'json',
    contentType: 'application/json',
    processData: false,
    // Allow to control the "number" field in the request
    data: SWITCH_ON_LIGHT.format(nb_lights),
    success: function (data) {
      console.log((JSON.stringify(data)));
    },
    error: function(){
      console.log("Impossible to send light command");
    }
  });
}
function add_log(message){
  var date_object = new Date();
  var date = date_object.getTime();
  var container = document.getElementById('log_list');
  var date_string = date_object.getHours() + "h" + date_object.getMinutes() + "m" + date_object.getSeconds() + "s";
  var li = document.createElement('li');
  var lis = container.getElementsByTagName("li");
  li.innerHTML =  date_string + ' : ' + message;
  container.insertBefore(li, container.firstChild);
  if(lis.length > LOG_LIMIT_SZ){
    container.removeChild(container.lastChild);
  }
}
function send_mail(mail){
  $.ajax({
    url: "https://" + SERVER_ADDR + "/api/v0/email",
    beforeSend: function(xhr) {
    },
    type: 'POST',
    headers:{
      "Accept": "application/json",
      "X-API-KEY": API_KEY
    },
    dataType: 'json',
    contentType: 'application/json',
    processData: false,
    // Allow to control the "number" field in the request
    data: SEND_MAIL.format(mail),
    success: function (data) {
      console.log("Mail send");
    },
    error: function(){
      console.log("Impossible to send mail");
    }
  });
}
function rule1_callback(){
  console.log("rule_1 callback");
  if(r1_sendmail){
    //update_rule_mail("rule_1", rule1_id, false);
    send_mail(rule1_mailto);
    add_log('Automatic rule : mail send to ' + rule1_mailto);
  }else{
    //update_rule_threshold("rule_1", rule1_id, rule1_operator, rule1_threshold, false);
    var number_to_print = 0;
    if(rule1_action == "switch on"){
      number_to_print = parseInt(current_light) + parseInt(rule1_lightnumber);
    }else{
      number_to_print = parseInt(current_light) - parseInt(rule1_lightnumber);
    }
    number_to_print = Math.min(number_to_print, 4);
    number_to_print = Math.max(number_to_print, 0);
    light_request(number_to_print);
    add_log('Automatic rule : ' + rule1_action + ' ' + number_to_print + ' lights');
  }
  //document.getElementById("rule1_activate").checked = false;
}
function rule2_callback(){
  console.log("rule_2 callback");
  if(r2_sendmail){
    //update_rule_mail("rule_2", rule2_id, false);
    send_mail(rule2_mailto);
    add_log('Automatic rule : mail send to ' + rule2_mailto);
  }else{
    //update_rule_threshold("rule_2", rule2_id, rule2_operator, rule2_threshold, false);
    var number_to_print = 0;
    if(rule2_action == "switch on"){
      number_to_print = parseInt(current_light) + parseInt(rule2_lightnumber);
    }else{
      number_to_print = parseInt(current_light) - parseInt(rule2_lightnumber);
    }
    number_to_print = Math.min(number_to_print, 4);
    number_to_print = Math.max(number_to_print, 0);
    light_request(number_to_print);
    add_log('Automatic rule : ' + rule2_action + ' ' + number_to_print + ' lights');
  }
  //document.getElementById("rule2_activate").checked = false;
}
function update_rule_mail(name, id, enabled){
  console.log("Update mail rule " + name);
  $.ajax({
    url: "https://" + SERVER_ADDR + "/api/v0/eventprocessing/matching-rule/" + id,
    beforeSend: function(xhr) {
    },
    type: 'PUT',
    headers:{
      "Accept": "application/json",
      "X-API-KEY": API_KEY
    },
    dataType: 'json',
    contentType: 'application/json',
    processData: false,
    data: UPDATE_RULE_MAIL.format(id, name, enabled),
    success: function (data) {
      console.log(name + " updated");
    },
    error: function(){
      console.log(UPDATE_RULE_MAIL.format(id, name, enabled))
      console.log("Impossible to update " + name);
    }
  });
}
function update_rule_threshold(name, id, operator, threshold, enabled){
  console.log("Update threshold rule " + name + "operator " + operator);
  $.ajax({
    url: "https://" + SERVER_ADDR + "/api/v0/eventprocessing/matching-rule/" + id,
    beforeSend: function(xhr) {
    },
    type: 'PUT',
    headers:{
      "Accept": "application/json",
      "X-API-KEY": API_KEY
    },
    dataType: 'json',
    contentType: 'application/json',
    processData: false,
    data: UPDATE_RULE_THRESHOLD.format(id, name, enabled, operator, threshold),
    success: function (data) {
      console.log(name + " updated");
    },
    error: function(){
      console.log(UPDATE_RULE_THRESHOLD.format(id, name, enabled, operator, threshold));
      console.log("Impossible to update " + name);
    }
  });
}
/**
* Easy way to format string, thanks to : http://stackoverflow.com/questions/610406/javascript-equivalent-to-printf-string-format
*
*/
// First, checks if it isn't implemented yet.
if (!String.prototype.format) {
  String.prototype.format = function() {
    var args = arguments;
    return this.replace(/{(\d+)}/g, function(match, number) {
      return typeof args[number] != 'undefined'
      ? args[number]
      : match
      ;
    });
  };
}

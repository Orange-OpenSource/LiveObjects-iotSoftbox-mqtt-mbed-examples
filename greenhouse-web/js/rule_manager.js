/**
*
* WARNING : COMMON.js SHOULD BE LOADED BEFORE THIS FILE, TO ALLOW
* AN ACCESS TO ITS CONSTANTS AND FUNCTION
*/

'use strict';

var r1_sendmail = false;
var r2_sendmail = false;

var SET_RULE_THRESHOLD = '{\
  "name": "{0}",\
  "enabled": true,\
  "dataPredicate": {\
    "{1}" : [ { "var" : "value.LightSensor" }, {2} ]\
  }\
}'

var SET_FIRING_RULE = '{\
  "name": "{0}",\
  "enabled": true,\
  "matchingRuleIds": ["{1}"],\
  "firingType":"{2}",\
  "sleepDuration":"PT10S"\
}'

function send_set_matching_rule(name, id, type){
  $.ajax({
    url: "https://" + SERVER_ADDR + "/api/v0/eventprocessing/firing-rule",
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
    data: SET_FIRING_RULE.format(name,id, type),

    success: function (data) {
      console.log((JSON.stringify(data)));
    },
    error: function(){
      console.log("Impossible to set firing rule")
    }
  });
}

function send_set_rule_threshold_request(name, operator, value){

  $.ajax({
    url: "https://" + SERVER_ADDR + "/api/v0/eventprocessing/matching-rule",
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
    data: SET_RULE_THRESHOLD.format(name,operator, value),

    success: function (data) {
      console.log((JSON.stringify(data)));
      var id = data["id"];
      if(name == "rule_1"){
        rule1_id = id;
      }else if(name == "rule_2"){
        rule2_id = id;
      }
      send_set_matching_rule(name, id, "SLEEP")
    },
    error: function(){
      console.log("Impossible to set matching rule")
    }
  });

}

/*
* Allow to manipulate the automatic rules
*/
function apply_rule1(){
  rule1_operator = document.getElementById("rule1_operator").value;
  if(r1_sendmail){
    rule1_mailto = document.getElementById("rule1_mailto").value;
    update_rule_mail("rule_1", rule1_id, true);
  }else{
    rule1_threshold = document.getElementById("rule1_threshold").value;
    rule1_lightnumber = document.getElementById("rule1_lightnumber").value;
    rule1_action = document.getElementById("rule1_action").value;
    update_rule_threshold("rule_1", rule1_id, rule1_operator, rule1_threshold, true);
  }
  rule1_active = true;
  document.getElementById("rule1_activate").checked = true
  save_context();
}

function apply_rule2(){
  rule2_operator = document.getElementById("rule2_operator").value;
  if(r2_sendmail){
    rule2_mailto = document.getElementById("rule2_mailto").value;
    update_rule_mail("rule_2", rule2_id, true);
  }else{
    rule2_threshold = document.getElementById("rule2_threshold").value;
    rule2_lightnumber = document.getElementById("rule2_lightnumber").value;
    rule2_action = document.getElementById("rule2_action").value;
    update_rule_threshold("rule_2", rule2_id, rule2_operator, rule2_threshold, true);
  }
  rule2_active = true;
  document.getElementById("rule2_activate").checked = true;
  save_context();
}

function rule1_activate(){
  var enabled = document.getElementById("rule1_activate").checked;
  rule1_active = enabled;
  if(r1_sendmail){
    update_rule_mail("rule_1", rule1_id, enabled);
  }else{
    update_rule_threshold("rule_1", rule1_id, rule1_operator, rule1_threshold, enabled);
  }
  save_context();
}

function rule2_activate(){
  var enabled = document.getElementById("rule2_activate").checked;
  rule2_active = enabled;
  if(r2_sendmail){
    update_rule_mail("rule_2", rule2_id, enabled);
  }else{
    update_rule_threshold("rule_2", rule2_id, rule2_operator, rule2_threshold, enabled);
  }
  save_context();
}

function threshold_form(rule){
  if(rule == "rule1"){
    r1_sendmail = false;
  }else{
    r2_sendmail = false;
  }
  document.getElementById(rule + '_lux_input').style.visibility='visible';
  var form_container = document.getElementById('action_form_' + rule);
  form_container.innerHTML = 'do \
  <select class="form-control"  id="' + rule +'_action">\
  <option>switch on</option>\
  <option>switch off</option>\
  </select>\
  <select class="form-control"  id="' + rule +'_lightnumber">\
  <option>1</option>\
  <option>2</option>\
  <option>3</option>\
  <option>4</option>\
  </select>\
  additional lights'
}

function mail_form(rule){
  if(rule == "rule1"){
    r1_sendmail = true;
  }else{
    r2_sendmail = true;
  }
  document.getElementById(rule + '_lux_input').style.visibility='hidden';
  var form_container = document.getElementById('action_form_' + rule);
  form_container.innerHTML = 'do\
  send mail to\
  <input type="email" size="28" class="form-control" id="' + rule +'_mailto">'
}

function set_rule_id(){
  $.ajax({
    url: "https://" + SERVER_ADDR + "/api/v0/eventprocessing/matching-rule",
    beforeSend: function(xhr) {
    },
    type: 'GET',
    headers:{
      "Accept": "application/json",
      "X-API-KEY": API_KEY
    },
    dataType: 'json',
    contentType: 'application/json',
    processData: false,
    data: "",
    success: function (data) {

      for(var i = 0; i < data.length; i++){
        if(data[i]["name"] == "rule_1"){
          rule1_id = data[i]["id"];
          rule1_active = data[i]["enabled"];
          document.getElementById("rule1_activate").checked = rule1_active;
        }
        else if(data[i]["name"] == "rule_2"){
          rule2_id = data[i]["id"];
          rule2_active = data[i]["enabled"];
          document.getElementById("rule2_activate").checked = rule2_active;
        }
      }

      //Set fake (matching-rule, firing-rule)
      if(rule1_id == 0){
        console.log("rule1 hasn't been set yet")
        send_set_rule_threshold_request("rule_1", "==", -1);
      }if(rule2_id == 0){
        console.log("rule2 hasn't been set yet")
        send_set_rule_threshold_request("rule_2", "==", -1);
      }
    },
    error: function(){
      console.log("Fail request rules list")
    }
  });
}
function change_form(rule_name, type){
  if(type=="<=" || type==">="){
    threshold_form(rule_name);
  }else if(type=="mail"){
    mail_form(rule_name);
  }
}

function init_rule(){
  if(new_loading()){
    threshold_form("rule1");
    threshold_form("rule2");
    set_rule_id();
    save_context();
  }else{
    retrieve_context();
  }
}

function new_loading(){
  if(sessionStorage.getItem('value_save') == null || sessionStorage.getItem('rule1_id') == 0 || sessionStorage.getItem('rule2_id') == 0){
    return true;
  }
  return false;
}

function save_context(){
  value_save = true;

  sessionStorage.setItem('rule1_active', rule1_active);
  sessionStorage.setItem('rule2_active', rule2_active);

  sessionStorage.setItem('current_light', current_light);
  sessionStorage.setItem('value_save', value_save);
  sessionStorage.setItem('rule1_id', rule1_id);
  sessionStorage.setItem('rule2_id', rule2_id);

  sessionStorage.setItem('rule1_operator', rule1_operator);
  sessionStorage.setItem('rule1_mailto', rule1_mailto);
  sessionStorage.setItem('rule1_threshold', rule1_threshold);
  sessionStorage.setItem('rule1_lightnumber', rule1_lightnumber);
  sessionStorage.setItem('rule1_action', rule1_action);

  sessionStorage.setItem('rule2_operator', rule2_operator);
  sessionStorage.setItem('rule2_mailto', rule2_mailto);
  sessionStorage.setItem('rule2_threshold', rule2_threshold);
  sessionStorage.setItem('rule2_lightnumber', rule2_lightnumber);
  sessionStorage.setItem('rule2_action', rule2_action);

  sessionStorage.setItem('r1_sendmail', r1_sendmail);
  sessionStorage.setItem('r2_sendmail', r2_sendmail);

}

function retrieve_context(){

  rule1_active = sessionStorage.getItem('rule1_active');
  rule2_active = sessionStorage.getItem('rule2_active');

  current_light = sessionStorage.getItem('current_light');

  value_save = sessionStorage.getItem('value_save');
  rule1_id = sessionStorage.getItem('rule1_id');
  rule2_id = sessionStorage.getItem('rule2_id');

  rule1_operator = sessionStorage.getItem('rule1_operator');
  rule1_mailto = sessionStorage.getItem('rule1_mailto');
  rule1_threshold = sessionStorage.getItem('rule1_threshold');
  rule1_lightnumber = sessionStorage.getItem('rule1_lightnumber');
  rule1_action = sessionStorage.getItem('rule1_action');
  r1_sendmail = sessionStorage.getItem('r1_sendmail');

  rule2_operator = sessionStorage.getItem('rule2_operator');
  rule2_mailto = sessionStorage.getItem('rule2_mailto');
  rule2_threshold = sessionStorage.getItem('rule2_threshold');
  rule2_lightnumber = sessionStorage.getItem('rule2_lightnumber');
  rule2_action = sessionStorage.getItem('rule2_action');
  r2_sendmail = sessionStorage.getItem('r2_sendmail');

  rule1_threshold = rule1_threshold == -1 ? "" : rule1_threshold;
  rule2_threshold = rule2_threshold == -1 ? "" : rule2_threshold;

  rule1_active = rule1_active == "true" ? true : false;
  rule2_active = rule2_active == "true" ? true : false;

  if(r1_sendmail == "true"){
    mail_form("rule1");
    document.getElementById("rule1_operator").value = "mail";
    document.getElementById("rule1_mailto").value = rule1_mailto;
  }
  else{
    threshold_form("rule1");
    document.getElementById("rule1_operator").value = rule1_operator;
    document.getElementById("rule1_threshold").value = rule1_threshold;
    document.getElementById("rule1_action").value = rule1_action;
    document.getElementById("rule1_lightnumber").value = rule1_lightnumber;
  }
  document.getElementById("rule1_activate").checked = rule1_active;

  if(r2_sendmail == "true"){
    mail_form("rule2");
    document.getElementById("rule2_operator").value = "mail";
    document.getElementById("rule2_mailto").value = rule2_mailto;
  }
  else{
    threshold_form("rule2");
    document.getElementById("rule2_operator").value = rule2_operator;
    document.getElementById("rule2_threshold").value = rule2_threshold;
    document.getElementById("rule2_action").value = rule2_action;
    document.getElementById("rule2_lightnumber").value = rule2_lightnumber;
  }
  document.getElementById("rule2_activate").checked = rule2_active;
}

init_rule();

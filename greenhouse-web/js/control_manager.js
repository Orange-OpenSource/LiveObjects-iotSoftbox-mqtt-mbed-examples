/**
*
* WARNING : COMMON.js SHOULD BE LOADED BEFORE THIS FILE, TO ALLOW
* AN ACCESS TO ITS CONSTANTS AND FUNCTION
*/

'use strict';

// in seconds
var UPDATE_VALUE = 1;

//The maximum number of alert allowed in the alert pannel
var LOG_LIMIT_SZ = 6
var DYNAMIC_PANEL_SZ = 393;

//TODO : check the right init value
var threshold = 0
var data_to_print = [];
var real_time = true;
var display_mode = "hour";
var plot;
var wait_init = true;

var i=0;
var mean_value = 0;
var max_value = 0;
var min_value = 100000000000;

var plot;


/**
*
* Here the http REST request already created
*
*/

var SORTED_LUMINOSITY_REQUEST_DSL = '{\
  "from" : {0}, "size" : {1},\
  "query" : {\
    "term" : {"streamId" : "{2}"}\
  },\
  "sort": { "timestamp": { "order": "desc" } },\
  "_source": ["timestamp", "streamId", "value"]\
}';

var CHANGE_NIGHT_MODE = '{\
  "night_mode": {"type":"UINT32",\
  "valueUInt32":{0}\
}\
}';



/**
* Global variable
*/
var further_start;
var periodic_interval;

var current_start_date;
var current_end_date;
var current_window;

var current_desired_hour;

/*
* Functions manipulationg the html content
*/
function add_mean_max_min(mean, max, min){
  var container_mean = document.getElementById('mean_value');
  var container_min = document.getElementById('min_value');
  var container_max = document.getElementById('max_value');

  container_mean.innerHTML = mean;
  container_min.innerHTML = min;
  container_max.innerHTML = max;
}


function set_night_mode_threshold(){
  threshold = document.getElementById("night_mode_input").value;
}


function set_light(number){
  current_light = number;
  var container = document.getElementById('light_container');
  container.innerHTML = ""
  for(var i=0; i< number; i++){
    container.innerHTML += '<div class="col-lg-2"><img src="../dist/css/light_bulb.png" alt="Logo" style="width:40px;height:40; background:transparent;"></div>'
  }

}

function print_luminosity(start_date, end_date){
  //console.log("print_luminosity : ");
  data_to_print = [];
  var last_i;
  var previous_date = new Date().getTime();
  mean_value = 0;
  max_value = 0;
  min_value = 1000000000;
  var count = 0;
  for(var i = 0; i < luminosity_collector.length; i++){
    last_i = i;
    //console.log(luminosity_collector[i]["date"].toString());
    var current_date = luminosity_collector[i]["date"];
    if(previous_date <= current_date){
      console.log("Error date")
    }
    previous_date = current_date;
    // If we have past the end_date
    if(current_date.getTime() < current_end_date.getTime()){
      break;
    }

    //if it's not too soon
    if(current_date.getTime() <= current_start_date.getTime()){
      data_to_print.push([current_date.getTime(), luminosity_collector[i]["value"]["LightSensor"]]);
      max_value = Math.max(luminosity_collector[i]["value"]["LightSensor"], max_value)
      min_value = Math.min(luminosity_collector[i]["value"]["LightSensor"], min_value)
      mean_value += luminosity_collector[i]["value"]["LightSensor"];
      count++;
    }
  }

  //console.log("print")

  plot.shutdown();
  plot = $.plot("#real_time_luminosity", [ data_to_print ], {
    series: {
      shadowSize: 0	// Drawing is faster without shadows
    },
    xaxis: {
      mode: "time",
      timezone: "browser",
      minTickSize: [1, display_mode],
      timeformat: "%b %d %H:%M",
      min: end_date,
      max: start_date,
      twelveHourClock: true
    },

    yaxis:{
      min: 0,
      max: 100
    },
    grid: {
      borderColor: null
    }
  });

  plot.draw();

  if(count == 0){
    mean_value = 0;
    max_value = 0;
    min_value = 0;
  }else{
    mean_value = mean_value/count;
  }
  add_mean_max_min(Math.floor(mean_value),Math.floor(max_value),Math.floor(min_value));
  // We put the last value in the dashboard view
  document.getElementById("luminosity_last_value").innerHTML = Math.floor(luminosity_collector[0]["value"]["LightSensor"]) + "%";
}

/**
* Functions used to send the Request to the REST API
*/
function send_lights_command(){
  var nb_lights = document.getElementById("lights_number_input").value;
  //Send the right request to order the lights on
  light_request(nb_lights);
  // Add the command to the history list
  add_log('Manual "Lights on for '+ nb_lights + ' lights"')
}


function send_night_command(){
  var message_string = 'Manual "Night mode send"'
  var send_value;
  send_value = 1;
  $.ajax({
    url: "https://" + SERVER_ADDR + "/api/v0/assets/"+ ASSET_NAMESPACE + "/" + ASSET_ID + "/params",
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
    data: CHANGE_NIGHT_MODE.format(send_value),

    success: function (data) {
      console.log((JSON.stringify(data)));
    },
    error: function(){
    }
  });

  add_log(message_string)
}


/**
* Functions used to manipulate the graph
*/
//Allow to search for luminosity value from [present_time, desired_hour]
// if request is true, the values will be charge from LOM server. If not, values will be the same from the last querry
function get_luminosity_values(desired_minute){
  //console.log("get luminosity")
  real_time = true;
  display_mode = "hour";
  var buffer = []
  current_window = desired_minute;
  current_start_date = new Date();
  // We compute the end date we want for the measures
  current_end_date = new Date(current_start_date.getTime()-desired_minute*60*1000);
  request_luminosity(1000, 0, current_end_date, buffer);
}

function zoom(desired_windows){
  console.log("Zoom");
  display_mode = "minute";
  real_time = true;
  print_luminosity_from_now(desired_windows)
}

function go_realtime(){
  console.log("go realtime");
  real_time = true;
}

function go_future(minute){
  console.log("Future");
  real_time = false;
  // We shitf the current start and end date
  current_start_date = new Date(current_start_date.getTime()+minute*60*1000);
  current_end_date = new Date(current_end_date.getTime()+minute*60*1000);
  print_luminosity(current_start_date, current_end_date);
}

function go_past(minute){
  console.log("Go past");
  real_time = false;
  // We shitf the current start and end date
  current_start_date = new Date(current_start_date.getTime()-minute*60*1000);
  current_end_date = new Date(current_end_date.getTime()-minute*60*1000);
  print_luminosity(current_start_date, current_end_date);

}

function request_luminosity(interval, start, desirated_date, buffer){
  //console.log("Start : " + start + " interval " + interval);
  further_start = start;
  var data_string;
  var last_timestamp;
  var current_timestamp;
  // Will get luminosity until we have reach all the datas we wanted
  $.ajax({
    url: "https://" + SERVER_ADDR + "/api/v0/data/search/hits",
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
    data: SORTED_LUMINOSITY_REQUEST_DSL.format(start, interval, STREAM_ID),
    success: function (data) {
      if(data.length == 0){
        //console.log("Buffer complete");
        terminate_request(buffer, desirated_date);
        return;
      }

      for (var i = 0; i < data.length; i++) {
        current_timestamp = data[i]['timestamp'];
        var last_date = new Date(current_timestamp);

        // if not
        buffer.push({"date":last_date, "value":data[i]['value']});

      }
      // if we have less than the requested values
      if(data.length != interval || start >= 1000){
        //means that we have all the values we want/can
        terminate_request(buffer, desirated_date);
      }else{
        //means that we have to go further
        request_luminosity(interval, start+interval, desirated_date, buffer);
      }

    },
    error: function(){
      console.log("Fail request")
    }
  });
}

// Allow to terminate the request and save the buffered values
function terminate_request(buffer, desirated_date){
  //console.log("terminate request")
  luminosity_collector = buffer;
  wait_init = false;
  print_luminosity(current_start_date, current_end_date);
  // We go real time
  zoom(1);
  real_time = true;
  // If the periodic update hasn't been launch
}


//periodic function used to shift the graph
function update_luminosity(){
  //console.log("update_luminosity");
  if(wait_init){
    return;
  }
  if(real_time){
    print_luminosity_from_now(current_window);
  }

}



function display_luminosity_panel(){
    document.getElementById("dashboard_panel").style.visibility="hidden";
    document.getElementById("dashboard_panel").style.height="0";
    document.getElementById("luminosity_panel").style.height=DYNAMIC_PANEL_SZ + "px";
    document.getElementById("luminosity_panel").style.visibility="visible";
}

function display_dashboard_panel(){
  document.getElementById("luminosity_panel").style.visibility="hidden";
  document.getElementById("luminosity_panel").style.height="0";
  document.getElementById("dashboard_panel").style.height=DYNAMIC_PANEL_SZ + "px";
  document.getElementById("dashboard_panel").style.visibility="visible";
}

/*
* Initialisation
*/
function init(){
  plot = $.plot("#real_time_luminosity", [ data_to_print ], {
    series: {
      shadowSize: 0	// Drawing is faster without shadows
    },
    xaxis: {
      mode: "time",
      timezone: "browser",
      minTickSize: [1, display_mode],
      min: (new Date()).getTime()-12*60*60*1000,
      max: (new Date()).getTime(),
      twelveHourClock: true
    }
  });

  get_luminosity_values(48*60, false);
  set_light(0);
  display_dashboard_panel();
  var interval = setInterval(update_luminosity, UPDATE_VALUE*1000);

}

init();

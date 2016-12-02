'use strict';

/**
*
* WARNING : COMMON.js SHOULD BE LOADED BEFORE THIS FILE, TO ALLOW
* AN ACCESS TO ITS CONSTANTS AND FUNCTION
*/

/*
*
* The string values for sending to the REST API
*
*/
var CHANGE_COORDINATES = '{\
  "longitude": {"type":"FLOAT",\
  "valueFloat":{0}\
},\
"latitude": {"type":"FLOAT",\
"valueFloat":{1}\
}\
}';

var CHANGE_COORDINATES_INT = '{\
  "lon_int32": {"type":"INT32",\
  "valueInt32":{0}\
},\
"lat_int32": {"type":"INT32",\
"valueInt32":{1}\
}\
}';

var CHANGE_PERIOD = '{\
  "period": {"type":"UINT32",\
  "valueUInt32":{0}\
}\
}';


var RESET = '{\
  "event": "RESET",\
  "payload": "FF09F8F09E09",\
  "data": {\
  }\
}';

function reset_device(){

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
    data: RESET,
    success: function (data) {
      console.log((JSON.stringify(data)));
    },
    error: function(){
      console.log("Impossible to send ");
    }
  });

  //add_log('Manual "Reset device"');
}


var map;
// A value somewhere in the USA
var myLatLng = {
  lat: 37.3303,
  lng: -121.889
};

var marker;
function initMap() {

  map = new google.maps.Map(document.getElementById('map-canvas'), {
    center: myLatLng,
    zoom: 12,
    //mapTypeId: 'satellite',
    disableDefaultUI: true
  });

  marker = new google.maps.Marker({
    position: myLatLng,
    map: map,
    title: 'Position'
  });

  map.setCenter(myLatLng);


}

function get_configuration(){
  // We delete the last caracter (the 's')
  var update_frequency = document.getElementById("frequency_input").value.slice(0, -1);
  var latitude = document.getElementById("latitude_input").value;
  var longitude = document.getElementById("longitude_input").value;

  change_coordinates(Math.floor(latitude*100000), Math.floor(longitude*100000));
  // if don't send if empty
  if(update_frequency != ''){
    send_update_frequency(update_frequency);
  }

  if(latitude != '' && longitude != ''){

    myLatLng["lat"] = parseFloat(latitude)
    myLatLng["lng"] = parseFloat(longitude)

    if(typeof marker !== 'undefined'){
      marker.setMap(null)
    }
    marker = new google.maps.Marker({
      position: myLatLng,
      map: map,
      title: 'Position'
    });

    map.setCenter(myLatLng);
  }

}

function send_update_frequency(update_frequency){

  $.ajax({
    url: "https://"+ SERVER_ADDR +"/api/v0/assets/"+ ASSET_NAMESPACE + "/" + ASSET_ID + "/params",
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
    data: CHANGE_PERIOD.format(update_frequency),

    success: function (data) {
      console.log((JSON.stringify(data)));
    },
    error: function(){
    }
  });
}

function change_coordinates(latitude, longitude){
  $.ajax({
    url: "https://"+ SERVER_ADDR +"/api/v0/assets/"+ ASSET_NAMESPACE + "/" + ASSET_ID + "/params",
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
    data: CHANGE_COORDINATES_INT.format(longitude, latitude),

    success: function (data) {
      console.log((JSON.stringify(data)));
    },
    error: function(){
    }
  });
}

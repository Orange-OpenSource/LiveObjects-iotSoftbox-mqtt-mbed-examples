var MQTT_ADDR = "wss://"+SERVER_ADDR+"/mqtt";
var MQTT_PORT = 1885;
var MQTT_USER = "json+bridge";
var MQTT_PSWD = API_KEY;
var MQTT_CLIENID = random_client_id();
var MEASURES_TOPIC = "fifo/webapp"
var EVENT_TOPIC = "router/~event/v1/data/eventprocessing/fired"


function random_client_id()
{
  var text = "";
  var possible = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

  for( var i=0; i < 10; i++ )
  text += possible.charAt(Math.floor(Math.random() * possible.length));
  return text;
}


var client
var interval

var options =
{
  userName: MQTT_USER,

  password: MQTT_PSWD,

  keepAliveInterval: 20,

  //connection attempt timeout in seconds
  timeout: 120,

  //Gets Called if the connection has successfully been established
  onSuccess: function onConnect() {
    console.log("Connected to LiveObjects");
    // Once a connection has been made, make a subscription and send a message.
    client.subscribe(MEASURES_TOPIC);
    client.subscribe(EVENT_TOPIC);

    // Allow to keep the connection alive (I have not find another solution ...)

  },

  //Gets Called if the connection could not be established
  onFailure: function (message) {
    console.log("Connection MQTT failed: " + message.errorMessage);
  }
};

connect_mqtt();

// called when the client loses its connection
function onConnectionLost(responseObject) {
  alert("Connexion lost");
  if (responseObject.errorCode !== 0) {
    console.log("onConnectionLost:"+responseObject.errorMessage);
  }
}

// called when a message arrives

function onMessageArrived(message) {
  message_json = JSON.parse(message.payloadString);
  if(message.destinationName == EVENT_TOPIC){
    var message_payload = JSON.parse(message_json["payload"])
    var rule_name = message_payload["matchingContext"]["matchingRule"]["name"];
    console.log("Matching : " + rule_name);
    if(rule_name == "rule_1"){
      rule1_callback();
    }else if (rule_name == "rule_2") {
      rule2_callback();
    }

  }else if(message.destinationName == MEASURES_TOPIC){
    if(message_json["streamId"] == STREAM_ID){
      message_date = new Date(message_json["timestamp"]);
      if(luminosity_collector.length > 0){
        if(luminosity_collector[0]["date"] < message_date.getTime()){
          console.log("New measures value");
          luminosity_collector.unshift({"date":message_date, "value":message_json['value']})
        }
      }
    }
    else if (message_json["streamId"] == STREAM_ID_LAMP) {
      set_light(message_json["value"].LEDS)
    }

    else if (message_json["streamId"] == STREAM_ID_ALERT) {
      if(message_json["value"]["LightSensorState"] == 0){
        add_log("Luminosity sensor failure")
      }else if(message_json["value"]["LightSensorState"] == 1){
        add_log("Luminosity sensor now working")
      }
    }
  }
}

function connect_mqtt(){
  console.log("Try connect mqtt")
  //Create a new Client object with your broker's hostname, port and your own clientId
  client = new Paho.MQTT.Client(MQTT_ADDR, MQTT_CLIENID);
  // set callback handlers
  client.onConnectionLost = onConnectionLost;
  client.onMessageArrived = onMessageArrived;
  client.connect(options);

}

function periodic_try_connection(){
  if(!client.isConnected()){
    console.log("Find client not connected");
    client.connect(options);
  }
}

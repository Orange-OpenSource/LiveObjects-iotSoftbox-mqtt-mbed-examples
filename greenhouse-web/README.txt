IOTSOFTBOX-DEMOGREENHOUSE


This is the standalone web-application used for the IoTSoftbox Greenhouse demo.

In order to connect the standalone web-application to the LiveObjects platform, make sure that the following variables are correctly set :

In the file js/common.js :

The variable SERVER_ADDR should contain the adress of your LiveObjects instance (IP or DNs name).
The variable API_KEY should contain the value of your API-Key you want to use for the demo.
The variable ASSET_ID should contain the MAC address of your device (used as device identifier).

To launch the demo, simply open index.html with firefox or chrome.
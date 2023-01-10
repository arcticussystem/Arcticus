# How to run the program please
# Please click on the pencil edit button to view the correct format and orientation
The server folder is to be located on the RPi and the client folder on the control station PC

To run server:
cd server/
sudo ./exe ip_client port_nr
To run delay calculating script server afterwards: 
Python3 calcAVG_server.py

To run client:
cd client/
sudo ./exe ip_server port_nr
To run delay calculating script client afterwards: 
Python3 calcAVG_client.py

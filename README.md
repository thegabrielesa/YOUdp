# YOUdp
File transfer on UDP. University of Rome, Tor Vergata. Project made for the exam of 'Ingegneria di Internet e Web' 2019.


            __________________________________________________
            __________________________________________________
            ________YY___YY__OOOO__U____U_____d_______________
            _________YY_YY__O____O_U____U_____d_______________
            ___________Y____O____O_U____U_ddddd_ppppp_________
            ___________Y____O____O_U____U_d___d_p___p_________
            ___________Y_____OOOO___UUUU__ddddd_ppppp_________
            ____________________________________p_____________
            ____________________________________p_____________
            __________________________________________________
                                YOUdp 1.0



These instructions will get you a copy of the project and running on your local 
machine for development and testing purposes.


## 1. Prerequisites

Please unzip the folder YOUdp in a path you choose and remember how it's 
located for the future time.


### 2.1 Server's initialization

To install the software you need to open a terminal on YOUdp folder and digit 
the command "make". After the compiling of the code, you are able to use the 
software.

If you want to run and test the software, please go into the folder SERVER, 
which has location in ".../YOUdp/SERVER", and open a terminal there.
To run server, you need to type "./server" and choose the parameters, if you 
want; otherwise default parameters will be WINDOW_SIZE=16, TIMEOUT=4 in 
adaptive mode and PROB of loss equal 0. If you need to choose those parameters 
you need to type like: 
```
./server -n 12 -p 5 -t 5 
```
where n is lenght's window, p is probability of loss and t is timeout's value 
in milliseconds.
If you choose the value of timeout, it doesn't go in adaptive mode. You can 
also type "-d" for debug mode, for testing purposes. 
Server has been configurated and is ready for use.


### 2.2 Client's initialization

It's possible to configurate the client. Please go into the folder CLIENT, 
which has location in ".../YOUDP/CLIENT", and open at least a terminal there.
To run client, you need to type "./client -h IP_address" and choose the 
parameters, if you want; otherwise default parameters will be WINDOW_SIZE=16, 
TIMEOUT=4 in adaptive mode and PROB of loss equal 0.
If you need to choose those parameters you need to type like: 
```
./client -h IP_address -n 12 -p 5 -t 5 
```
where n is window's lenght, p is probability of loss and t is timeout's value 
in milliseconds.

If you choose the value of timeout, it doesn't go in adaptive mode. You can 
also type "-d" for debug mode, for testing purposes. 
Client has been configurated and is ready for use.

Please, TAKE CARE of your previous choise for server and use the same 
parameters, otherwise client and server will not communicate in the right way.

For using it in localhost, you need to type address "127.0.0.1".
If you want you can open other clients and communicate with server.


### 3 Client's request

If you want to download files from server, it's better to type before the 
command "list". After that you can read which files are on the server and after 
you can type "get file_name".
If you want to upload files on server, you have to type "put file_name".
If you need to know the status of your download/upload, you need to use the 
command "stat". It shows the last 10 operations that you have made.


### 4 Quit

It's possibile to close client's console by typing the command "exit". It will 
terminate client's execution after the ending of any transmission.


### 5 File comparison

If it is necessary, you can compare two files to know if they are the same with 
the "file_compare" executable. It compares two files, given by their path, by 
each byte and returns if they are the same. It's only for testing purposes 
because files, that has been sent, are always the same.


Thanks for your attention. 


## Authors
* **thegabrielesa**
* **cesko**
* **tom**

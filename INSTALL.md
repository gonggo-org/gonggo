# Installing Gonggo on Linux:

1. Build cJSON. Download source code from https://github.com/DaveGamble/cJSON. The cJSON version used for current Gonggo development is 1.7.18.
   
   To build shared library, change directory to cJSON cloned directory:

   ```console
   cmake -DENABLE_CJSON_UTILS=On -DBUILD_SHARED_LIBS=On
   make
   sudo make install
   ```

2. Build CivetWeb. Download source code from https://github.com/civetweb/civetweb. The CivetWeb version used for current Gonggo development is 1.17.

   To build shared library with OpenSSL v3.0.x interface, please refer to [Building CivetWeb](https://github.com/civetweb/civetweb/blob/master/docs/Building.md):
   
   ```console
   make clean slib WITH_WEBSOCKET=1 COPT="-DOPENSSL_API_3_0"
   ``` 
   
   For secured web socket connection (wss), an SSL certificate is required. Refer to [Adding OpenSSL Support](https://github.com/civetweb/civetweb/blob/master/docs/OpenSSL.md) for an example of self-signed certificate.
	
	Once successfully built, CivetWeb has two important files:

    ```
	├── include
	│   ├── civetweb.h
	├── libcivetweb.so.1.17.0
	
	```
	
	Please manually create subdirectory civetweb under /usr/include and copy the **civetweb.h** into:
	
	```console
	sudo mkdir /usr/include/civetweb
	sudo cp ./include/civetweb.h /usr/include/civetweb
	```
	
	And copy the **libcivetweb.so.1.17.0** into the Linux architecture-dependent standard shared library directory (in my Linux installation is /usr/lib64) and create links:
	
	```console
	sudo cp ./libcivetweb.so.1.17.0 /usr/lib64/
    sudo ln -sf /usr/lib64/libcivetweb.so.1.17.0 /usr/lib64/libcivetweb.so.1
    sudo ln -sf /usr/lib64/libcivetweb.so.1 /usr/lib64/libcivetweb.so
	```

2. Gonggo depends on **glib2** and **sqlite3** shared libraries. Please install the respective development package.
	
3. Type `autoreconf -i` to remake the GNU Build System files in specified DIRECTORIES and their subdirectories.

4. Type `./configure` to create the configuration. A list of configure options is printed by running `configure --help`.

5. Then type `make` to build Gonggo.

6. Please modify the configuration file **./conf/gonggo.conf** and start-stop script file **./gonggoservice** to suit your Linux environment.

To start the Gonggo service:

   ```console
   sudo ./gonggoservice start
   ```
   
To stop the Gonggo service:

   ```console
   sudo ./gonggoservice stop
   ```

To check the status of the Gonggo service:

   ```console
   sudo ./gonggoservice status
   ```

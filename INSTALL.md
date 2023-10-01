# Installing Gonggo on Linux:

1. Build CivetWeb. Download source code from https://github.com/civetweb/civetweb. The CivetWeb version used for current Gonggo development is 1.16.

   To build shared library with GDB debug support turned on and OpenSSL v1.1.x interface, please refer to [Building CivetWeb](https://github.com/civetweb/civetweb/blob/master/docs/Building.md):
   
   ```console
   make clean slib WITH_WEBSOCKET=1 WITH_DEBUG=1 COPT="-DOPENSSL_API_1_1"
   ``` 
   
   For secured web socket connection (wss), an SSL certificate is required. Refer to [Adding OpenSSL Support](https://github.com/civetweb/civetweb/blob/master/docs/OpenSSL.md) for an example of self-signed certificate. For OpenSSL with minimum of 2048 private key length, please find a line on the web page:
    
    ```console
	openssl genrsa -des3 -out server.key 1024
	```
	
    Replace it with:
    ```console
	openssl genrsa -des3 -out server.key 2048
	```
	
	Once successfully built, CivetWeb has two important files:

    ```
	├── include
	│   ├── civetweb.h
	├── libcivetweb.so.1.16.0
	
	```
	
	Please manually create subdirectory civetweb under /usr/include and copy the **civetweb.h** into:
	
	```console
	sudo mkdir /usr/include/civetweb
	sudo cp ./include/civetweb.h /usr/include/civetweb
	```
	
	And copy the **libcivetweb.so.1.16.0** into the Linux architecture-dependent standard shared library directory (in my Linux installation is /usr/lib64) and rename the filename into **libcivetweb.so.1**:
	
	```console
	sudo cp ./libcivetweb.so.1.16.0 /usr/lib64/libcivetweb.so.1
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

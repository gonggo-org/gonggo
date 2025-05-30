# Installing Gonggo on Linux:

1. Build cJSON. Download source code from https://github.com/DaveGamble/cJSON. The cJSON version used for current Gonggo development is 1.7.18.
   
   To build shared library, change directory to cJSON cloned directory and build the package:

   ```console
   cmake -DENABLE_CJSON_UTILS=On -DBUILD_SHARED_LIBS=On
   make
   sudo make install
   ```

2. Build CivetWeb. Download source code from https://github.com/civetweb/civetweb. The CivetWeb version used for current Gonggo development is 1.17.

   To build shared library with OpenSSL v3.0.x interface, please refer to [Building CivetWeb](https://github.com/civetweb/civetweb/blob/master/docs/Building.md):
   
   ```console
   make clean slib WITH_IPV6=1 WITH_WEBSOCKET=1 SSL_LIB=libssl.so.3 CRYPTO_LIB=libcrypto.so.3 COPT="-DOPENSSL_API_3_0 -DNO_SSL_DL" LOPT="-lssl -lcrypto"
   ```

   With CivetWeb version 1.17, I encounter error message when stating -DOPENSSL_API_3_0:  "Multiple OPENSSL_API versions defined". The workaround is commenting out Makefile at line number 114:

   ```console
   #CFLAGS += -DOPENSSL_API_1_1
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

2. Gonggo depends on **uuid**, **glib2** and **sqlite3** shared libraries. Please install the respective development package.

   For Fedora and Rocky Linux with dnf package manager:

   ```console
   sudo dnf install libuuid-devel
   sudo dnf install glib2-devel
   sudo dnf install sqlite-devel
	```
	
3. Type `autoreconf -i` to remake the GNU Build System files in specified DIRECTORIES and their subdirectories.

4. Type `./configure` to create the configuration. A list of configure options is printed by running `configure --help`.

5. Then type `make` to build Gonggo.

6. Please modify the configuration file **./conf/gonggo.conf** and start-stop script file **./gonggoservice** to suit your Linux environment.

7. Service start and stop script comes in two versions:
    1. [Systemd](https://github.com/gonggo-org/gonggo/blob/main/scripts/gonggo.service).
    2. [Conservative](https://github.com/gonggo-org/gonggo/blob/main/scripts/gonggoservice).
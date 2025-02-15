# gonggo
A Lightweight Message Dispatcher over Web Socket

![gonggo](https://github.com/user-attachments/assets/7f0c4448-fc64-4658-99cd-fc7f8ba5bf1c)

Gonggo dispatches request from websocket client to the designated service. There are 2 types of request. First, a single shot request is submitted by client. Gonggo dispacthes it to the service. Then, asynchronously, the service pushes a response back to the client.

Second, a multi respond request (subscribe) is submitted by client. The service pushes responses continuously until an unsubscribe request is submitted.


## Mission

Gonggo simplifies data exchange between websocket clients and various backend services. 

- Gonggo provides single port for websocket clients to access various backend services.

- Gonggo regulates consistent client JSON request and response [JSON structure](https://html-preview.github.io/?url=https://github.com/gonggo-org/gonggo/blob/main/asyncapi/gonggospec/index.html).

## Dependencies

Gonggo is developed in C for efficient and fast message dispatching. It depends on libraries written in C: 
- [CivetWeb](https://github.com/civetweb/civetweb)
- [cJSON](https://github.com/DaveGamble/cJSON)

## Installation

Gonggo can only be installed on Linux server. [How to install](INSTALL.md).

A proxy must be developed for each of backend services. Gonggo provides shared library called **sawang** which encapsulates core data exchange module. All you have to do is defining 5 callback functions. [All about sawang](https://github.com/gonggo-org/sawang).

## Author
- Abdul Yadi (abdulyadi.datatrans@gmail.com)
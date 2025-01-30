# gonggo
A Lightweight Message Dispatcher over Web Socket

![gonggo](https://github.com/user-attachments/assets/7f0c4448-fc64-4658-99cd-fc7f8ba5bf1c)

Gonggo dispatches request from websocket client to the designated service. There are 2 types of request. First, a single shot request is submitted by client. Gonggo dispacthes it to the service. Then, asynchronously, the service pushes a response back to the client.

Second, a multi respond request (subscribe) is submitted by client. The service pushes responses continuosly until an unsubscribe request is submitted.

## Client JSON Specification

https://html-preview.github.io/?url=https://github.com/gonggo-org/gonggo/blob/main/asyncapi/gonggospec/index.html
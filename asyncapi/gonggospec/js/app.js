
    const schema = {
  "asyncapi": "3.0.0",
  "info": {
    "title": "Gonggo",
    "version": "1.0.0",
    "description": "Web socket message dispatcher"
  },
  "servers": {
    "development": {
      "host": "localhost:8877",
      "pathname": "/gonggo",
      "protocol": "wss",
      "description": "Web socket server for development"
    }
  },
  "operations": {
    "requestParsingStatus": {
      "description": "Client request parsing status",
      "action": "receive",
      "channel": {
        "address": "/",
        "messages": {
          "requestParsingStatus": {
            "name": "requestParsingStatus",
            "headers": {
              "description": "request parsing status header",
              "type": "object",
              "required": [
                "rid",
                "requestStatus"
              ],
              "properties": {
                "rid": {
                  "type": "string",
                  "description": "a unique UUID request id whom this message reply to",
                  "x-parser-schema-id": "<anonymous-schema-1>"
                },
                "requestStatus": {
                  "description": "request status id",
                  "type": "number",
                  "enum": [
                    1,
                    2,
                    3,
                    4,
                    5,
                    6,
                    7,
                    8,
                    9,
                    10,
                    11
                  ],
                  "x-requestStatusDescription": {
                    "1": "Request is Acknowledged",
                    "2": "Request Header is Missing",
                    "3": "Request Id is Missing",
                    "4": "Request Id is Overlength",
                    "5": "Request Service is Missing",
                    "6": "Request Service is Reserved",
                    "7": "Request Payload is Missing",
                    "8": "Request Payload is Invalid",
                    "9": "Request Gonggo Service Name is Invalid",
                    "10": "Request Timeout is Missing",
                    "11": "Request Timeout is Invalid"
                  },
                  "x-parser-schema-id": "<anonymous-schema-2>"
                }
              },
              "additionalProperties": false,
              "x-parser-schema-id": "requestParsingStatusHeader"
            },
            "examples": [
              {
                "headers": {
                  "rid": "e27fd7b9-808e-412b-8533-f5cc54ad613e",
                  "requestStatus": 1
                }
              }
            ],
            "x-parser-unique-object-id": "requestParsingStatus"
          },
          "requestExpiredNotification": {
            "name": "requestExpiredNotification",
            "headers": {
              "description": "request expired notification",
              "type": "object",
              "required": [
                "rid",
                "requestStatus"
              ],
              "properties": {
                "rid": {
                  "type": "string",
                  "description": "a unique UUID request id whom this message reply to",
                  "x-parser-schema-id": "<anonymous-schema-3>"
                },
                "requestStatus": {
                  "description": "request status id",
                  "type": "number",
                  "enum": [
                    22
                  ],
                  "x-requestStatusDescription": {
                    "22": "Request is Expired"
                  },
                  "x-parser-schema-id": "<anonymous-schema-4>"
                }
              },
              "additionalProperties": false,
              "x-parser-schema-id": "requestExpiredNotification"
            },
            "examples": [
              {
                "headers": {
                  "rid": "e27fd7b9-808e-412b-8533-f5cc54ad613e",
                  "requestStatus": 22
                }
              }
            ],
            "x-parser-unique-object-id": "requestExpiredNotification"
          },
          "gonggoTestSubmit": {
            "name": "gonggoTestSubmit",
            "headers": {
              "description": "gonggo test submit header",
              "allOf": [
                {
                  "title": "Request Submit Header Base",
                  "description": "header base for request submit",
                  "type": "object",
                  "required": [
                    "rid",
                    "service"
                  ],
                  "discriminator": "service",
                  "properties": {
                    "rid": {
                      "type": "string",
                      "description": "a unique UUID request id",
                      "maxLength": 36,
                      "x-parser-schema-id": "<anonymous-schema-5>"
                    },
                    "service": {
                      "type": "string",
                      "description": "the service name",
                      "x-parser-schema-id": "<anonymous-schema-6>"
                    }
                  },
                  "additionalProperties": false,
                  "x-parser-schema-id": "gonggoSubmitHeader"
                },
                {
                  "title": "Gonggo Test Submit Header",
                  "type": "object",
                  "required": [
                    "service"
                  ],
                  "properties": {
                    "service": {
                      "enum": [
                        "test"
                      ],
                      "x-parser-schema-id": "<anonymous-schema-8>"
                    }
                  },
                  "additionalProperties": false,
                  "x-parser-schema-id": "<anonymous-schema-7>"
                }
              ],
              "additionalProperties": false,
              "x-parser-schema-id": "gonggoTestSubmitHeader"
            },
            "examples": [
              {
                "headers": {
                  "rid": "e27fd7b9-808e-412b-8533-f5cc54ad613e",
                  "service": "test"
                }
              }
            ],
            "x-parser-unique-object-id": "gonggoTestSubmit"
          },
          "gonggoTestAnswer": {
            "name": "gonggoTestAnswer",
            "headers": {
              "description": "gonggo test answer header",
              "type": "object",
              "allOf": [
                {
                  "title": "Request Answer Status Header",
                  "description": "header for answered request",
                  "type": "object",
                  "required": [
                    "rid",
                    "requestStatus"
                  ],
                  "properties": {
                    "rid": {
                      "type": "string",
                      "description": "a unique UUID request id whom this message reply to",
                      "x-parser-schema-id": "<anonymous-schema-9>"
                    },
                    "requestStatus": {
                      "description": "request status id",
                      "type": "number",
                      "enum": [
                        20
                      ],
                      "x-requestStatusDescription": {
                        "20": "Request is Answered"
                      },
                      "x-parser-schema-id": "<anonymous-schema-10>"
                    }
                  },
                  "additionalProperties": false,
                  "x-parser-schema-id": "requestAnsweredStatusHeader"
                },
                {
                  "title": "Test Service Status",
                  "type": "object",
                  "required": [
                    "serviceStatus"
                  ],
                  "properties": {
                    "serviceStatus": {
                      "type": "number",
                      "enum": [
                        1
                      ],
                      "x-serviceStatusDescription": {
                        "1": "Test is success"
                      },
                      "x-parser-schema-id": "<anonymous-schema-12>"
                    }
                  },
                  "additionalProperties": false,
                  "x-parser-schema-id": "<anonymous-schema-11>"
                }
              ],
              "additionalProperties": false,
              "x-parser-schema-id": "gonggoTestAnswerHeader"
            },
            "payload": {
              "description": "gonggo test answer payload",
              "type": "object",
              "required": [
                "services",
                "proxies"
              ],
              "properties": {
                "services": {
                  "description": "array of gonggo services",
                  "type": "array",
                  "items": {
                    "type": "string",
                    "description": "the gonggo service name",
                    "x-parser-schema-id": "<anonymous-schema-14>"
                  },
                  "x-parser-schema-id": "<anonymous-schema-13>"
                },
                "proxies": {
                  "description": "array of active proxies",
                  "type": "array",
                  "items": {
                    "type": "string",
                    "description": "the proxy name",
                    "x-parser-schema-id": "<anonymous-schema-16>"
                  },
                  "x-parser-schema-id": "<anonymous-schema-15>"
                }
              },
              "additionalProperties": false,
              "x-parser-schema-id": "gonggoTestAnswerPayload"
            },
            "examples": [
              {
                "headers": {
                  "rid": "e27fd7b9-808e-412b-8533-f5cc54ad613e",
                  "requestStatus": 20,
                  "serviceStatus": 1
                },
                "payload": {
                  "services": [
                    "test",
                    "responseDump"
                  ],
                  "proxies": [
                    "dummy",
                    "database",
                    "machine"
                  ]
                }
              }
            ],
            "x-parser-unique-object-id": "gonggoTestAnswer"
          },
          "gonggoResponseDump": {
            "name": "gonggoResponseDumpSubmit",
            "headers": {
              "description": "gonggo response dump submit header",
              "allOf": [
                "$ref:$.operations.requestParsingStatus.channel.messages.gonggoTestSubmit.headers.allOf[0]",
                {
                  "title": "Response Dump Submit Header",
                  "type": "object",
                  "required": [
                    "service"
                  ],
                  "properties": {
                    "service": {
                      "enum": [
                        "requestDump"
                      ],
                      "x-parser-schema-id": "<anonymous-schema-18>"
                    }
                  },
                  "additionalProperties": false,
                  "x-parser-schema-id": "<anonymous-schema-17>"
                }
              ],
              "additionalProperties": false,
              "x-parser-schema-id": "gonggoResponseDumpSubmitHeader"
            },
            "payload": {
              "description": "gonggo response dump submit payload",
              "type": "object",
              "properties": {
                "rid": {
                  "type": "string",
                  "description": "a UUID request whose responses for dumping",
                  "x-parser-schema-id": "<anonymous-schema-19>"
                },
                "start": {
                  "type": "number",
                  "description": "the lowest one based index for responses dumping",
                  "minimum": 1,
                  "x-parser-schema-id": "<anonymous-schema-20>"
                },
                "size": {
                  "type": "number",
                  "description": "the page size for responses dumping",
                  "minimum": 1,
                  "x-parser-schema-id": "<anonymous-schema-21>"
                }
              },
              "additionalProperties": false,
              "x-parser-schema-id": "gonggoResponseDumpSubmitPayload"
            },
            "examples": [
              {
                "headers": {
                  "rid": "fb4cad20-2786-4356-aec3-9dae87883e92",
                  "service": "responseDump"
                },
                "payload": {
                  "rid": "e27fd7b9-808e-412b-8533-f5cc54ad613e",
                  "start": 1,
                  "size": 10
                }
              }
            ],
            "x-parser-unique-object-id": "gonggoResponseDump"
          },
          "gonggoResponseDumpAnswer": {
            "name": "gonggoResponseDumpAnswer",
            "headers": {
              "description": "gonggo response dump answer header",
              "type": "object",
              "allOf": [
                "$ref:$.operations.requestParsingStatus.channel.messages.gonggoTestAnswer.headers.allOf[0]",
                {
                  "title": "Response Dump Service Status",
                  "type": "object",
                  "required": [
                    "serviceStatus"
                  ],
                  "properties": {
                    "serviceStatus": {
                      "type": "number",
                      "enum": [
                        1,
                        2
                      ],
                      "x-serviceStatusDescription": {
                        "1": "Response Dump is Success",
                        "2": "Response Dump Database Error"
                      },
                      "x-parser-schema-id": "<anonymous-schema-23>"
                    }
                  },
                  "additionalProperties": false,
                  "x-parser-schema-id": "<anonymous-schema-22>"
                }
              ],
              "additionalProperties": false,
              "x-parser-schema-id": "gonggoResponseDumpAnswerHeader"
            },
            "payload": {
              "description": "gonggo response dump answer payload",
              "type": "object",
              "required": [
                "responses",
                "stop",
                "more"
              ],
              "properties": {
                "responses": {
                  "description": "array of responses",
                  "type": "array",
                  "items": {
                    "desription": "gonggo answer payload structure",
                    "type": "object",
                    "required": [
                      "headers",
                      "payload"
                    ],
                    "properties": {
                      "headers": {
                        "type": "object",
                        "x-parser-schema-id": "<anonymous-schema-25>"
                      },
                      "payload": {
                        "type": "object",
                        "x-parser-schema-id": "<anonymous-schema-26>"
                      }
                    },
                    "additionalProperties": false,
                    "x-parser-schema-id": "gonggoAnswerPayload"
                  },
                  "x-parser-schema-id": "<anonymous-schema-24>"
                },
                "stop": {
                  "type": "number",
                  "description": "the highest one based index of the dumped responses",
                  "minimum": 0,
                  "x-parser-schema-id": "<anonymous-schema-27>"
                },
                "more": {
                  "type": "number",
                  "description": "the remaining count of responses for next dump",
                  "minimum": 0,
                  "x-parser-schema-id": "<anonymous-schema-28>"
                }
              },
              "additionalProperties": false,
              "x-parser-schema-id": "gonggoResponseDumpAnswerPayload"
            },
            "examples": [
              {
                "headers": {
                  "rid": "fb4cad20-2786-4356-aec3-9dae87883e92",
                  "requestStatus": 20,
                  "serviceStatus": 1
                },
                "payload": {
                  "responses": [
                    {
                      "headers": {},
                      "payload": {}
                    }
                  ],
                  "stop": 15,
                  "more": 32
                }
              }
            ],
            "x-parser-unique-object-id": "gonggoResponseDumpAnswer"
          },
          "proxyAliveNotification": {
            "name": "proxyAliveNotification",
            "headers": {
              "description": "proxy alive notification header",
              "type": "object",
              "required": [
                "requestStatus",
                "serviceStatus"
              ],
              "properties": {
                "requestStatus": {
                  "description": "request status id",
                  "type": "number",
                  "enum": [
                    21
                  ],
                  "x-requestStatusDescription": {
                    "21": "Proxy Alive Notification"
                  },
                  "x-parser-schema-id": "<anonymous-schema-29>"
                },
                "serviceStatus": {
                  "type": "number",
                  "enum": [
                    1,
                    2
                  ],
                  "x-serviceStatusDescription": {
                    "1": "Proxy Start",
                    "2": "Proxy Termination"
                  },
                  "x-parser-schema-id": "<anonymous-schema-30>"
                }
              },
              "additionalProperties": false,
              "x-parser-schema-id": "proxyAliveNotificationHeader"
            },
            "payload": {
              "description": "proxy alive notification payload",
              "type": "object",
              "required": [
                "proxy",
                "rid"
              ],
              "properties": {
                "proxy": {
                  "type": "string",
                  "description": "the proxy name",
                  "x-parser-schema-id": "<anonymous-schema-31>"
                },
                "rid": {
                  "type": "array",
                  "items": {
                    "type": "string",
                    "description": "request id subscribing the proxy",
                    "x-parser-schema-id": "<anonymous-schema-33>"
                  },
                  "x-parser-schema-id": "<anonymous-schema-32>"
                }
              },
              "x-parser-schema-id": "proxyAliveNotificationPayload"
            },
            "examples": [
              {
                "headers": {
                  "requestStatus": 21,
                  "serviceStatus": 1
                },
                "payload": {
                  "proxy": "dummy",
                  "rid": [
                    "e27fd7b9-808e-412b-8533-f5cc54ad613e",
                    "fb4cad20-2786-4356-aec3-9dae87883e92"
                  ]
                }
              }
            ],
            "x-parser-unique-object-id": "proxyAliveNotification"
          },
          "proxySubscribe": {
            "name": "proxySubscribe",
            "headers": {
              "description": "proxy subscribe header",
              "allOf": [
                "$ref:$.operations.requestParsingStatus.channel.messages.gonggoTestSubmit.headers.allOf[0]",
                {
                  "title": "Proxy Subscribe Submit Header",
                  "type": "object",
                  "required": [
                    "service",
                    "proxy",
                    "timeout"
                  ],
                  "properties": {
                    "service": {
                      "description": "the service name specific to the proxy",
                      "x-parser-schema-id": "<anonymous-schema-35>"
                    },
                    "proxy": {
                      "type": "string",
                      "description": "the proxy name",
                      "minLength": 1,
                      "maxLength": 50,
                      "x-parser-schema-id": "<anonymous-schema-36>"
                    },
                    "timeout": {
                      "type": "integer",
                      "format": "int64",
                      "minimum": 1,
                      "description": "optional timeout in seconds",
                      "x-parser-schema-id": "<anonymous-schema-37>"
                    }
                  },
                  "additionalProperties": false,
                  "x-parser-schema-id": "<anonymous-schema-34>"
                }
              ],
              "additionalProperties": false,
              "x-parser-schema-id": "proxySubscribeHeader"
            },
            "payload": {
              "description": "Proxy subscribe payload. The payload structure is specific to the proxy.",
              "type": "object",
              "x-parser-schema-id": "proxySubscribePayload"
            },
            "examples": [
              {
                "headers": {
                  "rid": "fb4cad20-2786-4356-aec3-9dae87883e92",
                  "proxy": "dummy",
                  "service": "test",
                  "timeout": 21600
                },
                "payload": {
                  "message": "I am foo"
                }
              }
            ],
            "x-parser-unique-object-id": "proxySubscribe"
          },
          "proxyAnswer": {
            "name": "proxyAnswer",
            "headers": {
              "description": "proxy answer header",
              "type": "object",
              "allOf": [
                "$ref:$.operations.requestParsingStatus.channel.messages.gonggoTestAnswer.headers.allOf[0]",
                {
                  "title": "Proxy Answer Header",
                  "type": "object",
                  "required": [
                    "serviceStatus"
                  ],
                  "properties": {
                    "serviceStatus": {
                      "type": "number",
                      "description": "specific to the proxy service",
                      "minimum": 100,
                      "x-parser-schema-id": "<anonymous-schema-39>"
                    }
                  },
                  "additionalProperties": false,
                  "x-parser-schema-id": "<anonymous-schema-38>"
                }
              ],
              "additionalProperties": false,
              "x-parser-schema-id": "proxyAnswerHeader"
            },
            "payload": {
              "description": "Proxy answer payload. The payload structure is specific to the proxy.",
              "type": "object",
              "x-parser-schema-id": "proxyAnswerPayload"
            },
            "examples": [
              {
                "headers": {
                  "rid": "fb4cad20-2786-4356-aec3-9dae87883e92",
                  "requestStatus": 20,
                  "serviceStatus": 100
                },
                "payload": {
                  "reply": "hello foo"
                }
              }
            ],
            "x-parser-unique-object-id": "proxyAnswer"
          }
        },
        "x-parser-unique-object-id": "rootChannel"
      },
      "messages": [
        "$ref:$.operations.requestParsingStatus.channel.messages.requestParsingStatus"
      ],
      "x-parser-unique-object-id": "requestParsingStatus"
    },
    "requestExpiredNotification": {
      "description": "Client request is not responded after timeout elapsed",
      "action": "receive",
      "channel": "$ref:$.operations.requestParsingStatus.channel",
      "messages": [
        "$ref:$.operations.requestParsingStatus.channel.messages.requestExpiredNotification"
      ],
      "x-parser-unique-object-id": "requestExpiredNotification"
    },
    "gonggoTest": {
      "description": "Gonggo service availability test. The request should pass requestParsingStatus prior to gonngoTestAnswer.",
      "action": "send",
      "channel": "$ref:$.operations.requestParsingStatus.channel",
      "messages": [
        "$ref:$.operations.requestParsingStatus.channel.messages.gonggoTestSubmit"
      ],
      "x-parser-unique-object-id": "gonggoTest"
    },
    "gonggoTestAnswer": {
      "description": "The answer for gonggoTest.",
      "action": "receive",
      "channel": "$ref:$.operations.requestParsingStatus.channel",
      "messages": [
        "$ref:$.operations.requestParsingStatus.channel.messages.gonggoTestAnswer"
      ],
      "x-parser-unique-object-id": "gonggoTestAnswer"
    },
    "gonggoResponseDump": {
      "description": "Gonggo dumps all responses filtered by request UUID for client. The request should pass requestParsingStatus prior to gonngoResponseDumpAnswer.",
      "action": "send",
      "channel": "$ref:$.operations.requestParsingStatus.channel",
      "messages": [
        "$ref:$.operations.requestParsingStatus.channel.messages.gonggoResponseDump"
      ],
      "x-parser-unique-object-id": "gonggoResponseDump"
    },
    "gonggoResponseDumpAnswer": {
      "description": "The answer for gonggoResponseDump",
      "action": "receive",
      "channel": "$ref:$.operations.requestParsingStatus.channel",
      "messages": [
        "$ref:$.operations.requestParsingStatus.channel.messages.gonggoResponseDumpAnswer"
      ],
      "x-parser-unique-object-id": "gonggoResponseDumpAnswer"
    },
    "proxyAliveNotification": {
      "description": "Proxy start or termination notification for clients subscribing its service",
      "action": "receive",
      "channel": "$ref:$.operations.requestParsingStatus.channel",
      "messages": [
        "$ref:$.operations.requestParsingStatus.channel.messages.proxyAliveNotification"
      ],
      "x-parser-unique-object-id": "proxyAliveNotification"
    },
    "proxySubscribe": {
      "description": "Template for client subscribing any proxy service. The request should pass requestParsingStatus prior to proxyAnswer.",
      "action": "send",
      "channel": "$ref:$.operations.requestParsingStatus.channel",
      "messages": [
        "$ref:$.operations.requestParsingStatus.channel.messages.proxySubscribe"
      ],
      "x-parser-unique-object-id": "proxySubscribe"
    },
    "proxyAnswer": {
      "description": "Template for any proxy reply to proxySubscribe.",
      "action": "receive",
      "channel": "$ref:$.operations.requestParsingStatus.channel",
      "messages": [
        "$ref:$.operations.requestParsingStatus.channel.messages.proxyAnswer"
      ],
      "x-parser-unique-object-id": "proxyAnswer"
    }
  },
  "channels": {
    "rootChannel": "$ref:$.operations.requestParsingStatus.channel"
  },
  "components": {
    "messages": {
      "requestParsingStatus": "$ref:$.operations.requestParsingStatus.channel.messages.requestParsingStatus",
      "requestExpiredNotification": "$ref:$.operations.requestParsingStatus.channel.messages.requestExpiredNotification",
      "gonggoTestSubmit": "$ref:$.operations.requestParsingStatus.channel.messages.gonggoTestSubmit",
      "gonggoTestAnswer": "$ref:$.operations.requestParsingStatus.channel.messages.gonggoTestAnswer",
      "gonggoResponseDumpSubmit": "$ref:$.operations.requestParsingStatus.channel.messages.gonggoResponseDump",
      "gonggoResponseDumpAnswer": "$ref:$.operations.requestParsingStatus.channel.messages.gonggoResponseDumpAnswer",
      "proxyAliveNotification": "$ref:$.operations.requestParsingStatus.channel.messages.proxyAliveNotification",
      "proxySubscribe": "$ref:$.operations.requestParsingStatus.channel.messages.proxySubscribe",
      "proxyAnswer": "$ref:$.operations.requestParsingStatus.channel.messages.proxyAnswer"
    },
    "schemas": {
      "requestParsingStatusHeader": "$ref:$.operations.requestParsingStatus.channel.messages.requestParsingStatus.headers",
      "requestExpiredNotification": "$ref:$.operations.requestParsingStatus.channel.messages.requestExpiredNotification.headers",
      "requestAnsweredStatusHeader": "$ref:$.operations.requestParsingStatus.channel.messages.gonggoTestAnswer.headers.allOf[0]",
      "gonggoSubmitHeader": "$ref:$.operations.requestParsingStatus.channel.messages.gonggoTestSubmit.headers.allOf[0]",
      "gonggoTestSubmitHeader": "$ref:$.operations.requestParsingStatus.channel.messages.gonggoTestSubmit.headers",
      "gonggoTestAnswerHeader": "$ref:$.operations.requestParsingStatus.channel.messages.gonggoTestAnswer.headers",
      "gonggoTestAnswerPayload": "$ref:$.operations.requestParsingStatus.channel.messages.gonggoTestAnswer.payload",
      "gonggoResponseDumpSubmitHeader": "$ref:$.operations.requestParsingStatus.channel.messages.gonggoResponseDump.headers",
      "gonggoResponseDumpSubmitPayload": "$ref:$.operations.requestParsingStatus.channel.messages.gonggoResponseDump.payload",
      "gonggoResponseDumpAnswerHeader": "$ref:$.operations.requestParsingStatus.channel.messages.gonggoResponseDumpAnswer.headers",
      "gonggoResponseDumpAnswerPayload": "$ref:$.operations.requestParsingStatus.channel.messages.gonggoResponseDumpAnswer.payload",
      "gonggoAnswerPayload": "$ref:$.operations.requestParsingStatus.channel.messages.gonggoResponseDumpAnswer.payload.properties.responses.items",
      "proxyAliveNotificationHeader": "$ref:$.operations.requestParsingStatus.channel.messages.proxyAliveNotification.headers",
      "proxyAliveNotificationPayload": "$ref:$.operations.requestParsingStatus.channel.messages.proxyAliveNotification.payload",
      "proxySubscribeHeader": "$ref:$.operations.requestParsingStatus.channel.messages.proxySubscribe.headers",
      "proxySubscribePayload": "$ref:$.operations.requestParsingStatus.channel.messages.proxySubscribe.payload",
      "proxyAnswerHeader": "$ref:$.operations.requestParsingStatus.channel.messages.proxyAnswer.headers",
      "proxyAnswerPayload": "$ref:$.operations.requestParsingStatus.channel.messages.proxyAnswer.payload"
    }
  },
  "x-parser-spec-parsed": true,
  "x-parser-api-version": 3,
  "x-parser-spec-stringified": true
};
    const config = {"show":{"sidebar":true},"sidebar":{"showOperations":"byDefault"},"requestLabel":"SEND"};
    const appRoot = document.getElementById('root');
    AsyncApiStandalone.render(
        { schema, config, }, appRoot
    );
  
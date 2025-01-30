sudo asyncapi generate fromTemplate gonggo.json @asyncapi/html-template@3.1.0 --use-new-generator --force-write -o ./gonggospec --param config='{"requestLabel":"SEND"}' && sudo node ./gonggohotfix.js

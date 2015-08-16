var LRUCache = require('../index');
var cache = new LRUCache({ maxElements: 500 }); 
var buffer = [];
for(var i = 0; i < 1000; i++) {
    buffer.push(new Buffer(10000).fill(i));
}
console.time('test');
for(i = 0; i < 1000; i+=2) {
    cache.set(i, buffer[i]);
    for(var j = 0; j < 1000; j++) {
        cache.get(i);
    } 
}
console.timeEnd('test');
console.log(process.memoryUsage());
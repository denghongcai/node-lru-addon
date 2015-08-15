var LRUCache = require('../index');
var cache = new LRUCache({ maxElements: 5000 }); 
var buffer = [];
for(var i = 0; i < 5000; i++) {
    buffer.push(new Buffer(1000).fill(i));
}
console.time('test');
for(var i = 0; i < 5000; i+=2) {
    cache.set(i, buffer[i]);
}
for(i = 0; i < 5000; i++) {
    cache.get(i);
}           
console.timeEnd('test');
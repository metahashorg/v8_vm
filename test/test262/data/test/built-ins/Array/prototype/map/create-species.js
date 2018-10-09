// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.1.3.16
esid: sec-array.prototype.map
description: Species constructor is used to create a new instance
info: |
    [...]
    5. Let A be ? ArraySpeciesCreate(O, len).
    [...]

    9.4.2.3 ArraySpeciesCreate

    [...]
    5. Let C be ? Get(originalArray, "constructor").
    [...]
    7. If Type(C) is Object, then
       a. Let C be ? Get(C, @@species).
       b. If C is null, let C be undefined.
    [...]
    10. Return ? Construct(C, « length »).
features: [Symbol.species]
---*/

var thisValue, args, result;
var callCount = 0;
var instance = [];
var Ctor = function() {
  callCount += 1;
  thisValue = this;
  args = arguments;
  return instance;
};
var a = [1, 2, 3, 4, 5];
a.constructor = {};
a.constructor[Symbol.species] = Ctor;

result = a.map(function() {});

assert.sameValue(callCount, 1, 'Constructor invoked exactly once');
assert.sameValue(Object.getPrototypeOf(thisValue), Ctor.prototype);
assert.sameValue(args.length, 1, 'Constructor invoked with a single argument');
assert.sameValue(args[0], 5);
assert.sameValue(result, instance);

// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-5-b-164
description: >
    Object.defineProperties - value of 'writable' property of
    'descObj' is undefined (8.10.5 step 6.b)
includes: [propertyHelper.js]
---*/

var obj = {};

Object.defineProperties(obj, {
  property: {
    writable: undefined
  }
});

assert(obj.hasOwnProperty("property"));
verifyNotWritable(obj, "property");

'use strict';

let spectest = {
  print: console.log.bind(console),
  print_i32: console.log.bind(console),
  print_i32_f32: console.log.bind(console),
  print_f64_f64: console.log.bind(console),
  print_f32: console.log.bind(console),
  print_f64: console.log.bind(console),
  global_i32: 666,
  global_f32: 666,
  global_f64: 666,
  table: new WebAssembly.Table({initial: 10, maximum: 20, element: 'anyfunc'}),
  memory: new WebAssembly.Memory({initial: 1, maximum: 2})
};
let handler = {
  get(target, prop) {
    return (prop in target) ?  target[prop] : {};
  }
};
let registry = new Proxy({spectest}, handler);

function register(name, instance) {
  registry[name] = instance.exports;
}

function module(bytes, valid = true) {
  let buffer = new ArrayBuffer(bytes.length);
  let view = new Uint8Array(buffer);
  for (let i = 0; i < bytes.length; ++i) {
    view[i] = bytes.charCodeAt(i);
  }
  let validated;
  try {
    validated = WebAssembly.validate(buffer);
  } catch (e) {
    throw new Error("Wasm validate throws");
  }
  if (validated !== valid) {
    throw new Error("Wasm validate failure" + (valid ? "" : " expected"));
  }
  return new WebAssembly.Module(buffer);
}

function instance(bytes, imports = registry) {
  return new WebAssembly.Instance(module(bytes), imports);
}

function call(instance, name, args) {
  return instance.exports[name](...args);
}

function get(instance, name) {
  let v = instance.exports[name];
  return (v instanceof WebAssembly.Global) ? v.value : v;
}

function exports(name, instance) {
  return {[name]: instance.exports};
}

function run(action) {
  action();
}

function assert_malformed(bytes) {
  try { module(bytes, false) } catch (e) {
    if (e instanceof WebAssembly.CompileError) return;
  }
  throw new Error("Wasm decoding failure expected");
}

function assert_invalid(bytes) {
  try { module(bytes, false) } catch (e) {
    if (e instanceof WebAssembly.CompileError) return;
  }
  throw new Error("Wasm validation failure expected");
}

function assert_unlinkable(bytes) {
  let mod = module(bytes);
  try { new WebAssembly.Instance(mod, registry) } catch (e) {
    if (e instanceof WebAssembly.LinkError) return;
  }
  throw new Error("Wasm linking failure expected");
}

function assert_uninstantiable(bytes) {
  let mod = module(bytes);
  try { new WebAssembly.Instance(mod, registry) } catch (e) {
    if (e instanceof WebAssembly.RuntimeError) return;
  }
  throw new Error("Wasm trap expected");
}

function assert_trap(action) {
  try { action() } catch (e) {
    if (e instanceof WebAssembly.RuntimeError) return;
  }
  throw new Error("Wasm trap expected");
}

let StackOverflow;
try { (function f() { 1 + f() })() } catch (e) { StackOverflow = e.constructor }

function assert_exhaustion(action) {
  try { action() } catch (e) {
    if (e instanceof StackOverflow) return;
  }
  throw new Error("Wasm resource exhaustion expected");
}

function assert_return(action, expected) {
  let actual = action();
  if (!Object.is(actual, expected)) {
    throw new Error("Wasm return value " + expected + " expected, got " + actual);
  };
}

function assert_return_canonical_nan(action) {
  let actual = action();
  // Note that JS can't reliably distinguish different NaN values,
  // so there's no good way to test that it's a canonical NaN.
  if (!Number.isNaN(actual)) {
    throw new Error("Wasm return value NaN expected, got " + actual);
  };
}

function assert_return_arithmetic_nan(action) {
  // Note that JS can't reliably distinguish different NaN values,
  // so there's no good way to test for specific bitpatterns here.
  let actual = action();
  if (!Number.isNaN(actual)) {
    throw new Error("Wasm return value NaN expected, got " + actual);
  };
}


// call_indirect.wast:3
let $1 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\xf1\x80\x80\x80\x00\x16\x60\x00\x00\x60\x00\x01\x7f\x60\x00\x01\x7e\x60\x00\x01\x7d\x60\x00\x01\x7c\x60\x01\x7f\x01\x7f\x60\x01\x7e\x01\x7e\x60\x01\x7d\x01\x7d\x60\x01\x7c\x01\x7c\x60\x02\x7d\x7f\x01\x7f\x60\x02\x7f\x7e\x01\x7e\x60\x02\x7c\x7d\x01\x7d\x60\x02\x7e\x7c\x01\x7c\x60\x01\x7f\x01\x7f\x60\x01\x7e\x01\x7e\x60\x01\x7d\x01\x7d\x60\x01\x7c\x01\x7c\x60\x04\x7e\x7c\x7f\x7e\x01\x7f\x60\x01\x7e\x01\x7f\x60\x04\x7e\x7c\x7f\x7e\x00\x60\x01\x7e\x00\x60\x01\x7f\x01\x7e\x03\xa8\x80\x80\x80\x00\x27\x01\x02\x03\x04\x05\x06\x07\x08\x0a\x0c\x09\x0b\x0d\x0e\x0f\x10\x00\x01\x02\x03\x04\x02\x01\x02\x03\x04\x01\x02\x03\x04\x0a\x15\x06\x06\x05\x05\x00\x00\x00\x04\x85\x80\x80\x80\x00\x01\x70\x01\x17\x17\x07\x9b\x82\x80\x80\x00\x15\x08\x74\x79\x70\x65\x2d\x69\x33\x32\x00\x11\x08\x74\x79\x70\x65\x2d\x69\x36\x34\x00\x12\x08\x74\x79\x70\x65\x2d\x66\x33\x32\x00\x13\x08\x74\x79\x70\x65\x2d\x66\x36\x34\x00\x14\x0a\x74\x79\x70\x65\x2d\x69\x6e\x64\x65\x78\x00\x15\x0e\x74\x79\x70\x65\x2d\x66\x69\x72\x73\x74\x2d\x69\x33\x32\x00\x16\x0e\x74\x79\x70\x65\x2d\x66\x69\x72\x73\x74\x2d\x69\x36\x34\x00\x17\x0e\x74\x79\x70\x65\x2d\x66\x69\x72\x73\x74\x2d\x66\x33\x32\x00\x18\x0e\x74\x79\x70\x65\x2d\x66\x69\x72\x73\x74\x2d\x66\x36\x34\x00\x19\x0f\x74\x79\x70\x65\x2d\x73\x65\x63\x6f\x6e\x64\x2d\x69\x33\x32\x00\x1a\x0f\x74\x79\x70\x65\x2d\x73\x65\x63\x6f\x6e\x64\x2d\x69\x36\x34\x00\x1b\x0f\x74\x79\x70\x65\x2d\x73\x65\x63\x6f\x6e\x64\x2d\x66\x33\x32\x00\x1c\x0f\x74\x79\x70\x65\x2d\x73\x65\x63\x6f\x6e\x64\x2d\x66\x36\x34\x00\x1d\x08\x64\x69\x73\x70\x61\x74\x63\x68\x00\x1e\x13\x64\x69\x73\x70\x61\x74\x63\x68\x2d\x73\x74\x72\x75\x63\x74\x75\x72\x61\x6c\x00\x1f\x03\x66\x61\x63\x00\x20\x03\x66\x69\x62\x00\x21\x04\x65\x76\x65\x6e\x00\x22\x03\x6f\x64\x64\x00\x23\x07\x72\x75\x6e\x61\x77\x61\x79\x00\x24\x0e\x6d\x75\x74\x75\x61\x6c\x2d\x72\x75\x6e\x61\x77\x61\x79\x00\x25\x09\x9d\x80\x80\x80\x00\x01\x00\x41\x00\x0b\x17\x00\x01\x02\x03\x04\x05\x06\x07\x0a\x08\x0b\x09\x20\x21\x22\x23\x24\x25\x26\x0c\x0d\x0e\x0f\x0a\x90\x85\x80\x80\x00\x27\x85\x80\x80\x80\x00\x00\x41\xb2\x02\x0b\x85\x80\x80\x80\x00\x00\x42\xe4\x02\x0b\x87\x80\x80\x80\x00\x00\x43\x00\x20\x73\x45\x0b\x8b\x80\x80\x80\x00\x00\x44\x00\x00\x00\x00\x00\xc8\xae\x40\x0b\x84\x80\x80\x80\x00\x00\x20\x00\x0b\x84\x80\x80\x80\x00\x00\x20\x00\x0b\x84\x80\x80\x80\x00\x00\x20\x00\x0b\x84\x80\x80\x80\x00\x00\x20\x00\x0b\x84\x80\x80\x80\x00\x00\x20\x01\x0b\x84\x80\x80\x80\x00\x00\x20\x01\x0b\x84\x80\x80\x80\x00\x00\x20\x01\x0b\x84\x80\x80\x80\x00\x00\x20\x01\x0b\x84\x80\x80\x80\x00\x00\x20\x00\x0b\x84\x80\x80\x80\x00\x00\x20\x00\x0b\x84\x80\x80\x80\x00\x00\x20\x00\x0b\x84\x80\x80\x80\x00\x00\x20\x00\x0b\xdd\x80\x80\x80\x00\x00\x41\x00\x11\x00\x00\x42\x00\x41\x00\x11\x14\x00\x42\x00\x44\x00\x00\x00\x00\x00\x00\x00\x00\x41\x00\x42\x00\x41\x00\x11\x13\x00\x41\x00\x11\x00\x00\x41\x00\x11\x01\x00\x45\x1a\x41\x00\x11\x01\x00\x45\x1a\x42\x00\x41\x00\x11\x12\x00\x45\x1a\x42\x00\x44\x00\x00\x00\x00\x00\x00\x00\x00\x41\x00\x42\x00\x41\x00\x11\x11\x00\x45\x1a\x42\x00\x41\x00\x11\x06\x00\x50\x1a\x0b\x87\x80\x80\x80\x00\x00\x41\x00\x11\x01\x00\x0b\x87\x80\x80\x80\x00\x00\x41\x01\x11\x02\x00\x0b\x87\x80\x80\x80\x00\x00\x41\x02\x11\x03\x00\x0b\x87\x80\x80\x80\x00\x00\x41\x03\x11\x04\x00\x0b\x8a\x80\x80\x80\x00\x00\x42\xe4\x00\x41\x05\x11\x06\x00\x0b\x89\x80\x80\x80\x00\x00\x41\x20\x41\x04\x11\x05\x00\x0b\x8a\x80\x80\x80\x00\x00\x42\xc0\x00\x41\x05\x11\x06\x00\x0b\x8c\x80\x80\x80\x00\x00\x43\xc3\xf5\xa8\x3f\x41\x06\x11\x07\x00\x0b\x90\x80\x80\x80\x00\x00\x44\x3d\x0a\xd7\xa3\x70\x3d\xfa\x3f\x41\x07\x11\x08\x00\x0b\x8e\x80\x80\x80\x00\x00\x43\x66\x66\x00\x42\x41\x20\x41\x08\x11\x09\x00\x0b\x8c\x80\x80\x80\x00\x00\x41\x20\x42\xc0\x00\x41\x09\x11\x0a\x00\x0b\x95\x80\x80\x80\x00\x00\x44\x00\x00\x00\x00\x00\x00\x50\x40\x43\x00\x00\x00\x42\x41\x0a\x11\x0b\x00\x0b\x93\x80\x80\x80\x00\x00\x42\xc0\x00\x44\x66\x66\x66\x66\x66\x06\x50\x40\x41\x0b\x11\x0c\x00\x0b\x89\x80\x80\x80\x00\x00\x20\x01\x20\x00\x11\x06\x00\x0b\x89\x80\x80\x80\x00\x00\x42\x09\x20\x00\x11\x0e\x00\x0b\x98\x80\x80\x80\x00\x00\x20\x00\x50\x04\x7e\x42\x01\x05\x20\x00\x20\x00\x42\x01\x7d\x41\x0c\x11\x06\x00\x7e\x0b\x0b\xa2\x80\x80\x80\x00\x00\x20\x00\x42\x01\x58\x04\x7e\x42\x01\x05\x20\x00\x42\x02\x7d\x41\x0d\x11\x06\x00\x20\x00\x42\x01\x7d\x41\x0d\x11\x06\x00\x7c\x0b\x0b\x95\x80\x80\x80\x00\x00\x20\x00\x45\x04\x7f\x41\x2c\x05\x20\x00\x41\x01\x6b\x41\x0f\x11\x05\x00\x0b\x0b\x96\x80\x80\x80\x00\x00\x20\x00\x45\x04\x7f\x41\xe3\x00\x05\x20\x00\x41\x01\x6b\x41\x0e\x11\x05\x00\x0b\x0b\x87\x80\x80\x80\x00\x00\x41\x10\x11\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x41\x12\x11\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x41\x11\x11\x00\x00\x0b");

// call_indirect.wast:211
assert_return(() => call($1, "type-i32", []), 306);

// call_indirect.wast:212
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x88\x80\x80\x80\x00\x02\x60\x00\x00\x60\x00\x01\x7e\x02\x8f\x80\x80\x80\x00\x01\x02\x24\x31\x08\x74\x79\x70\x65\x2d\x69\x36\x34\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\x98\x80\x80\x80\x00\x01\x92\x80\x80\x80\x00\x00\x02\x40\x10\x00\x01\x42\xe4\x02\x01\x51\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "type-i64", []), int64("356"))

// call_indirect.wast:213
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x88\x80\x80\x80\x00\x02\x60\x00\x00\x60\x00\x01\x7d\x02\x8f\x80\x80\x80\x00\x01\x02\x24\x31\x08\x74\x79\x70\x65\x2d\x66\x33\x32\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\x9a\x80\x80\x80\x00\x01\x94\x80\x80\x80\x00\x00\x02\x40\x10\x00\xbc\x43\x00\x20\x73\x45\xbc\x46\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "type-f32", []), 3890.)

// call_indirect.wast:214
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x88\x80\x80\x80\x00\x02\x60\x00\x00\x60\x00\x01\x7c\x02\x8f\x80\x80\x80\x00\x01\x02\x24\x31\x08\x74\x79\x70\x65\x2d\x66\x36\x34\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\x9e\x80\x80\x80\x00\x01\x98\x80\x80\x80\x00\x00\x02\x40\x10\x00\xbd\x44\x00\x00\x00\x00\x00\xc8\xae\x40\xbd\x51\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "type-f64", []), 3940.)

// call_indirect.wast:216
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x88\x80\x80\x80\x00\x02\x60\x00\x00\x60\x00\x01\x7e\x02\x91\x80\x80\x80\x00\x01\x02\x24\x31\x0a\x74\x79\x70\x65\x2d\x69\x6e\x64\x65\x78\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\x98\x80\x80\x80\x00\x01\x92\x80\x80\x80\x00\x00\x02\x40\x10\x00\x01\x42\xe4\x00\x01\x51\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "type-index", []), int64("100"))

// call_indirect.wast:218
assert_return(() => call($1, "type-first-i32", []), 32);

// call_indirect.wast:219
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x88\x80\x80\x80\x00\x02\x60\x00\x00\x60\x00\x01\x7e\x02\x95\x80\x80\x80\x00\x01\x02\x24\x31\x0e\x74\x79\x70\x65\x2d\x66\x69\x72\x73\x74\x2d\x69\x36\x34\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\x98\x80\x80\x80\x00\x01\x92\x80\x80\x80\x00\x00\x02\x40\x10\x00\x01\x42\xc0\x00\x01\x51\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "type-first-i64", []), int64("64"))

// call_indirect.wast:220
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x88\x80\x80\x80\x00\x02\x60\x00\x00\x60\x00\x01\x7d\x02\x95\x80\x80\x80\x00\x01\x02\x24\x31\x0e\x74\x79\x70\x65\x2d\x66\x69\x72\x73\x74\x2d\x66\x33\x32\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\x9a\x80\x80\x80\x00\x01\x94\x80\x80\x80\x00\x00\x02\x40\x10\x00\xbc\x43\xc3\xf5\xa8\x3f\xbc\x46\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "type-first-f32", []), 1.32000005245)

// call_indirect.wast:221
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x88\x80\x80\x80\x00\x02\x60\x00\x00\x60\x00\x01\x7c\x02\x95\x80\x80\x80\x00\x01\x02\x24\x31\x0e\x74\x79\x70\x65\x2d\x66\x69\x72\x73\x74\x2d\x66\x36\x34\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\x9e\x80\x80\x80\x00\x01\x98\x80\x80\x80\x00\x00\x02\x40\x10\x00\xbd\x44\x3d\x0a\xd7\xa3\x70\x3d\xfa\x3f\xbd\x51\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "type-first-f64", []), 1.64)

// call_indirect.wast:223
assert_return(() => call($1, "type-second-i32", []), 32);

// call_indirect.wast:224
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x88\x80\x80\x80\x00\x02\x60\x00\x00\x60\x00\x01\x7e\x02\x96\x80\x80\x80\x00\x01\x02\x24\x31\x0f\x74\x79\x70\x65\x2d\x73\x65\x63\x6f\x6e\x64\x2d\x69\x36\x34\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\x98\x80\x80\x80\x00\x01\x92\x80\x80\x80\x00\x00\x02\x40\x10\x00\x01\x42\xc0\x00\x01\x51\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "type-second-i64", []), int64("64"))

// call_indirect.wast:225
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x88\x80\x80\x80\x00\x02\x60\x00\x00\x60\x00\x01\x7d\x02\x96\x80\x80\x80\x00\x01\x02\x24\x31\x0f\x74\x79\x70\x65\x2d\x73\x65\x63\x6f\x6e\x64\x2d\x66\x33\x32\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\x9a\x80\x80\x80\x00\x01\x94\x80\x80\x80\x00\x00\x02\x40\x10\x00\xbc\x43\x00\x00\x00\x42\xbc\x46\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "type-second-f32", []), 32.)

// call_indirect.wast:226
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x88\x80\x80\x80\x00\x02\x60\x00\x00\x60\x00\x01\x7c\x02\x96\x80\x80\x80\x00\x01\x02\x24\x31\x0f\x74\x79\x70\x65\x2d\x73\x65\x63\x6f\x6e\x64\x2d\x66\x36\x34\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\x9e\x80\x80\x80\x00\x01\x98\x80\x80\x80\x00\x00\x02\x40\x10\x00\xbd\x44\x66\x66\x66\x66\x66\x06\x50\x40\xbd\x51\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "type-second-f64", []), 64.1)

// call_indirect.wast:228
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x00\x00\x60\x02\x7f\x7e\x01\x7e\x02\x8f\x80\x80\x80\x00\x01\x02\x24\x31\x08\x64\x69\x73\x70\x61\x74\x63\x68\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\x9b\x80\x80\x80\x00\x01\x95\x80\x80\x80\x00\x00\x02\x40\x41\x05\x42\x02\x10\x00\x01\x42\x02\x01\x51\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "dispatch", [5, int64("2")]), int64("2"))

// call_indirect.wast:229
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x00\x00\x60\x02\x7f\x7e\x01\x7e\x02\x8f\x80\x80\x80\x00\x01\x02\x24\x31\x08\x64\x69\x73\x70\x61\x74\x63\x68\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\x9b\x80\x80\x80\x00\x01\x95\x80\x80\x80\x00\x00\x02\x40\x41\x05\x42\x05\x10\x00\x01\x42\x05\x01\x51\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "dispatch", [5, int64("5")]), int64("5"))

// call_indirect.wast:230
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x00\x00\x60\x02\x7f\x7e\x01\x7e\x02\x8f\x80\x80\x80\x00\x01\x02\x24\x31\x08\x64\x69\x73\x70\x61\x74\x63\x68\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\x9c\x80\x80\x80\x00\x01\x96\x80\x80\x80\x00\x00\x02\x40\x41\x0c\x42\x05\x10\x00\x01\x42\xf8\x00\x01\x51\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "dispatch", [12, int64("5")]), int64("120"))

// call_indirect.wast:231
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x00\x00\x60\x02\x7f\x7e\x01\x7e\x02\x8f\x80\x80\x80\x00\x01\x02\x24\x31\x08\x64\x69\x73\x70\x61\x74\x63\x68\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\x9b\x80\x80\x80\x00\x01\x95\x80\x80\x80\x00\x00\x02\x40\x41\x0d\x42\x05\x10\x00\x01\x42\x08\x01\x51\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "dispatch", [13, int64("5")]), int64("8"))

// call_indirect.wast:232
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x00\x00\x60\x02\x7f\x7e\x01\x7e\x02\x8f\x80\x80\x80\x00\x01\x02\x24\x31\x08\x64\x69\x73\x70\x61\x74\x63\x68\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\x9b\x80\x80\x80\x00\x01\x95\x80\x80\x80\x00\x00\x02\x40\x41\x14\x42\x02\x10\x00\x01\x42\x02\x01\x51\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "dispatch", [20, int64("2")]), int64("2"))

// call_indirect.wast:233
assert_trap(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x00\x00\x60\x02\x7f\x7e\x01\x7e\x02\x8f\x80\x80\x80\x00\x01\x02\x24\x31\x08\x64\x69\x73\x70\x61\x74\x63\x68\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\x93\x80\x80\x80\x00\x01\x8d\x80\x80\x80\x00\x00\x02\x40\x41\x00\x42\x02\x10\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_trap(() => call($1, "dispatch", [0, int64("2")]))

// call_indirect.wast:234
assert_trap(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x00\x00\x60\x02\x7f\x7e\x01\x7e\x02\x8f\x80\x80\x80\x00\x01\x02\x24\x31\x08\x64\x69\x73\x70\x61\x74\x63\x68\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\x93\x80\x80\x80\x00\x01\x8d\x80\x80\x80\x00\x00\x02\x40\x41\x0f\x42\x02\x10\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_trap(() => call($1, "dispatch", [15, int64("2")]))

// call_indirect.wast:235
assert_trap(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x00\x00\x60\x02\x7f\x7e\x01\x7e\x02\x8f\x80\x80\x80\x00\x01\x02\x24\x31\x08\x64\x69\x73\x70\x61\x74\x63\x68\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\x93\x80\x80\x80\x00\x01\x8d\x80\x80\x80\x00\x00\x02\x40\x41\x17\x42\x02\x10\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_trap(() => call($1, "dispatch", [23, int64("2")]))

// call_indirect.wast:236
assert_trap(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x00\x00\x60\x02\x7f\x7e\x01\x7e\x02\x8f\x80\x80\x80\x00\x01\x02\x24\x31\x08\x64\x69\x73\x70\x61\x74\x63\x68\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\x93\x80\x80\x80\x00\x01\x8d\x80\x80\x80\x00\x00\x02\x40\x41\x7f\x42\x02\x10\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_trap(() => call($1, "dispatch", [-1, int64("2")]))

// call_indirect.wast:237
assert_trap(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x00\x00\x60\x02\x7f\x7e\x01\x7e\x02\x8f\x80\x80\x80\x00\x01\x02\x24\x31\x08\x64\x69\x73\x70\x61\x74\x63\x68\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\x97\x80\x80\x80\x00\x01\x91\x80\x80\x80\x00\x00\x02\x40\x41\xe7\x84\xce\xc2\x04\x42\x02\x10\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_trap(() => call($1, "dispatch", [1213432423, int64("2")]))

// call_indirect.wast:239
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x7f\x01\x7e\x02\x9a\x80\x80\x80\x00\x01\x02\x24\x31\x13\x64\x69\x73\x70\x61\x74\x63\x68\x2d\x73\x74\x72\x75\x63\x74\x75\x72\x61\x6c\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\x99\x80\x80\x80\x00\x01\x93\x80\x80\x80\x00\x00\x02\x40\x41\x05\x10\x00\x01\x42\x09\x01\x51\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "dispatch-structural", [5]), int64("9"))

// call_indirect.wast:240
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x7f\x01\x7e\x02\x9a\x80\x80\x80\x00\x01\x02\x24\x31\x13\x64\x69\x73\x70\x61\x74\x63\x68\x2d\x73\x74\x72\x75\x63\x74\x75\x72\x61\x6c\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\x99\x80\x80\x80\x00\x01\x93\x80\x80\x80\x00\x00\x02\x40\x41\x05\x10\x00\x01\x42\x09\x01\x51\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "dispatch-structural", [5]), int64("9"))

// call_indirect.wast:241
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x7f\x01\x7e\x02\x9a\x80\x80\x80\x00\x01\x02\x24\x31\x13\x64\x69\x73\x70\x61\x74\x63\x68\x2d\x73\x74\x72\x75\x63\x74\x75\x72\x61\x6c\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\x9b\x80\x80\x80\x00\x01\x95\x80\x80\x80\x00\x00\x02\x40\x41\x0c\x10\x00\x01\x42\x80\x93\x16\x01\x51\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "dispatch-structural", [12]), int64("362880"))

// call_indirect.wast:242
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x7f\x01\x7e\x02\x9a\x80\x80\x80\x00\x01\x02\x24\x31\x13\x64\x69\x73\x70\x61\x74\x63\x68\x2d\x73\x74\x72\x75\x63\x74\x75\x72\x61\x6c\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\x99\x80\x80\x80\x00\x01\x93\x80\x80\x80\x00\x00\x02\x40\x41\x14\x10\x00\x01\x42\x09\x01\x51\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "dispatch-structural", [20]), int64("9"))

// call_indirect.wast:243
assert_trap(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x7f\x01\x7e\x02\x9a\x80\x80\x80\x00\x01\x02\x24\x31\x13\x64\x69\x73\x70\x61\x74\x63\x68\x2d\x73\x74\x72\x75\x63\x74\x75\x72\x61\x6c\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\x91\x80\x80\x80\x00\x01\x8b\x80\x80\x80\x00\x00\x02\x40\x41\x0b\x10\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_trap(() => call($1, "dispatch-structural", [11]))

// call_indirect.wast:244
assert_trap(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x7f\x01\x7e\x02\x9a\x80\x80\x80\x00\x01\x02\x24\x31\x13\x64\x69\x73\x70\x61\x74\x63\x68\x2d\x73\x74\x72\x75\x63\x74\x75\x72\x61\x6c\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\x91\x80\x80\x80\x00\x01\x8b\x80\x80\x80\x00\x00\x02\x40\x41\x16\x10\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_trap(() => call($1, "dispatch-structural", [22]))

// call_indirect.wast:246
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x7e\x01\x7e\x02\x8a\x80\x80\x80\x00\x01\x02\x24\x31\x03\x66\x61\x63\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\x99\x80\x80\x80\x00\x01\x93\x80\x80\x80\x00\x00\x02\x40\x42\x00\x10\x00\x01\x42\x01\x01\x51\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "fac", [int64("0")]), int64("1"))

// call_indirect.wast:247
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x7e\x01\x7e\x02\x8a\x80\x80\x80\x00\x01\x02\x24\x31\x03\x66\x61\x63\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\x99\x80\x80\x80\x00\x01\x93\x80\x80\x80\x00\x00\x02\x40\x42\x01\x10\x00\x01\x42\x01\x01\x51\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "fac", [int64("1")]), int64("1"))

// call_indirect.wast:248
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x7e\x01\x7e\x02\x8a\x80\x80\x80\x00\x01\x02\x24\x31\x03\x66\x61\x63\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\x9a\x80\x80\x80\x00\x01\x94\x80\x80\x80\x00\x00\x02\x40\x42\x05\x10\x00\x01\x42\xf8\x00\x01\x51\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "fac", [int64("5")]), int64("120"))

// call_indirect.wast:249
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x7e\x01\x7e\x02\x8a\x80\x80\x80\x00\x01\x02\x24\x31\x03\x66\x61\x63\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\xa2\x80\x80\x80\x00\x01\x9c\x80\x80\x80\x00\x00\x02\x40\x42\x19\x10\x00\x01\x42\x80\x80\x80\xde\x87\x92\xec\xcf\xe1\x00\x01\x51\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "fac", [int64("25")]), int64("7034535277573963776"))

// call_indirect.wast:251
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x7e\x01\x7e\x02\x8a\x80\x80\x80\x00\x01\x02\x24\x31\x03\x66\x69\x62\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\x99\x80\x80\x80\x00\x01\x93\x80\x80\x80\x00\x00\x02\x40\x42\x00\x10\x00\x01\x42\x01\x01\x51\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "fib", [int64("0")]), int64("1"))

// call_indirect.wast:252
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x7e\x01\x7e\x02\x8a\x80\x80\x80\x00\x01\x02\x24\x31\x03\x66\x69\x62\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\x99\x80\x80\x80\x00\x01\x93\x80\x80\x80\x00\x00\x02\x40\x42\x01\x10\x00\x01\x42\x01\x01\x51\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "fib", [int64("1")]), int64("1"))

// call_indirect.wast:253
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x7e\x01\x7e\x02\x8a\x80\x80\x80\x00\x01\x02\x24\x31\x03\x66\x69\x62\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\x99\x80\x80\x80\x00\x01\x93\x80\x80\x80\x00\x00\x02\x40\x42\x02\x10\x00\x01\x42\x02\x01\x51\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "fib", [int64("2")]), int64("2"))

// call_indirect.wast:254
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x7e\x01\x7e\x02\x8a\x80\x80\x80\x00\x01\x02\x24\x31\x03\x66\x69\x62\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\x99\x80\x80\x80\x00\x01\x93\x80\x80\x80\x00\x00\x02\x40\x42\x05\x10\x00\x01\x42\x08\x01\x51\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "fib", [int64("5")]), int64("8"))

// call_indirect.wast:255
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x7e\x01\x7e\x02\x8a\x80\x80\x80\x00\x01\x02\x24\x31\x03\x66\x69\x62\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\x9b\x80\x80\x80\x00\x01\x95\x80\x80\x80\x00\x00\x02\x40\x42\x14\x10\x00\x01\x42\xc2\xd5\x00\x01\x51\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "fib", [int64("20")]), int64("10946"))

// call_indirect.wast:257
assert_return(() => call($1, "even", [0]), 44);

// call_indirect.wast:258
assert_return(() => call($1, "even", [1]), 99);

// call_indirect.wast:259
assert_return(() => call($1, "even", [100]), 44);

// call_indirect.wast:260
assert_return(() => call($1, "even", [77]), 99);

// call_indirect.wast:261
assert_return(() => call($1, "odd", [0]), 99);

// call_indirect.wast:262
assert_return(() => call($1, "odd", [1]), 44);

// call_indirect.wast:263
assert_return(() => call($1, "odd", [200]), 99);

// call_indirect.wast:264
assert_return(() => call($1, "odd", [77]), 44);

// call_indirect.wast:266
assert_exhaustion(() => call($1, "runaway", []));

// call_indirect.wast:267
assert_exhaustion(() => call($1, "mutual-runaway", []));

// call_indirect.wast:272
assert_malformed("\x3c\x6d\x61\x6c\x66\x6f\x72\x6d\x65\x64\x20\x71\x75\x6f\x74\x65\x3e");

// call_indirect.wast:284
assert_malformed("\x3c\x6d\x61\x6c\x66\x6f\x72\x6d\x65\x64\x20\x71\x75\x6f\x74\x65\x3e");

// call_indirect.wast:296
assert_malformed("\x3c\x6d\x61\x6c\x66\x6f\x72\x6d\x65\x64\x20\x71\x75\x6f\x74\x65\x3e");

// call_indirect.wast:308
assert_malformed("\x3c\x6d\x61\x6c\x66\x6f\x72\x6d\x65\x64\x20\x71\x75\x6f\x74\x65\x3e");

// call_indirect.wast:320
assert_malformed("\x3c\x6d\x61\x6c\x66\x6f\x72\x6d\x65\x64\x20\x71\x75\x6f\x74\x65\x3e");

// call_indirect.wast:332
assert_malformed("\x3c\x6d\x61\x6c\x66\x6f\x72\x6d\x65\x64\x20\x71\x75\x6f\x74\x65\x3e");

// call_indirect.wast:342
assert_malformed("\x3c\x6d\x61\x6c\x66\x6f\x72\x6d\x65\x64\x20\x71\x75\x6f\x74\x65\x3e");

// call_indirect.wast:349
assert_malformed("\x3c\x6d\x61\x6c\x66\x6f\x72\x6d\x65\x64\x20\x71\x75\x6f\x74\x65\x3e");

// call_indirect.wast:359
assert_malformed("\x3c\x6d\x61\x6c\x66\x6f\x72\x6d\x65\x64\x20\x71\x75\x6f\x74\x65\x3e");

// call_indirect.wast:369
assert_malformed("\x3c\x6d\x61\x6c\x66\x6f\x72\x6d\x65\x64\x20\x71\x75\x6f\x74\x65\x3e");

// call_indirect.wast:379
assert_malformed("\x3c\x6d\x61\x6c\x66\x6f\x72\x6d\x65\x64\x20\x71\x75\x6f\x74\x65\x3e");

// call_indirect.wast:394
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x0a\x8d\x80\x80\x80\x00\x01\x87\x80\x80\x80\x00\x00\x41\x00\x11\x00\x00\x0b");

// call_indirect.wast:402
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x04\x84\x80\x80\x80\x00\x01\x70\x00\x00\x0a\x8e\x80\x80\x80\x00\x01\x88\x80\x80\x80\x00\x00\x41\x00\x11\x00\x00\x45\x0b");

// call_indirect.wast:410
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x88\x80\x80\x80\x00\x02\x60\x00\x01\x7e\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x01\x04\x84\x80\x80\x80\x00\x01\x70\x00\x00\x0a\x8e\x80\x80\x80\x00\x01\x88\x80\x80\x80\x00\x00\x41\x00\x11\x00\x00\x45\x0b");

// call_indirect.wast:419
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x88\x80\x80\x80\x00\x02\x60\x01\x7f\x00\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x01\x04\x84\x80\x80\x80\x00\x01\x70\x00\x00\x0a\x8d\x80\x80\x80\x00\x01\x87\x80\x80\x80\x00\x00\x41\x00\x11\x00\x00\x0b");

// call_indirect.wast:427
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x02\x7c\x7f\x00\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x01\x04\x84\x80\x80\x80\x00\x01\x70\x00\x00\x0a\x8d\x80\x80\x80\x00\x01\x87\x80\x80\x80\x00\x00\x41\x00\x11\x00\x00\x0b");

// call_indirect.wast:435
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x04\x84\x80\x80\x80\x00\x01\x70\x00\x00\x0a\x8f\x80\x80\x80\x00\x01\x89\x80\x80\x80\x00\x00\x41\x01\x41\x00\x11\x00\x00\x0b");

// call_indirect.wast:443
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x04\x84\x80\x80\x80\x00\x01\x70\x00\x00\x0a\x98\x80\x80\x80\x00\x01\x92\x80\x80\x80\x00\x00\x44\x00\x00\x00\x00\x00\x00\x00\x40\x41\x01\x41\x00\x11\x00\x00\x0b");

// call_indirect.wast:454
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x88\x80\x80\x80\x00\x02\x60\x01\x7f\x00\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x01\x04\x84\x80\x80\x80\x00\x01\x70\x00\x00\x0a\x8e\x80\x80\x80\x00\x01\x88\x80\x80\x80\x00\x00\x41\x01\x01\x11\x00\x00\x0b");

// call_indirect.wast:462
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x88\x80\x80\x80\x00\x02\x60\x01\x7f\x00\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x01\x04\x84\x80\x80\x80\x00\x01\x70\x00\x00\x0a\x8f\x80\x80\x80\x00\x01\x89\x80\x80\x80\x00\x00\x41\x00\x42\x01\x11\x00\x00\x0b");

// call_indirect.wast:471
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x02\x7f\x7f\x00\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x01\x04\x84\x80\x80\x80\x00\x01\x70\x00\x00\x0a\x90\x80\x80\x80\x00\x01\x8a\x80\x80\x80\x00\x00\x01\x41\x01\x41\x00\x11\x00\x00\x0b");

// call_indirect.wast:481
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x02\x7f\x7f\x00\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x01\x04\x84\x80\x80\x80\x00\x01\x70\x00\x00\x0a\x90\x80\x80\x80\x00\x01\x8a\x80\x80\x80\x00\x00\x41\x01\x01\x41\x00\x11\x00\x00\x0b");

// call_indirect.wast:491
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x02\x7f\x7c\x00\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x01\x04\x84\x80\x80\x80\x00\x01\x70\x00\x00\x0a\x98\x80\x80\x80\x00\x01\x92\x80\x80\x80\x00\x00\x44\x00\x00\x00\x00\x00\x00\xf0\x3f\x41\x01\x41\x00\x11\x00\x00\x0b");

// call_indirect.wast:501
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x02\x7c\x7f\x00\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x01\x04\x84\x80\x80\x80\x00\x01\x70\x00\x00\x0a\x98\x80\x80\x80\x00\x01\x92\x80\x80\x80\x00\x00\x41\x01\x44\x00\x00\x00\x00\x00\x00\xf0\x3f\x41\x00\x11\x00\x00\x0b");

// call_indirect.wast:515
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x04\x84\x80\x80\x80\x00\x01\x70\x00\x00\x0a\x8d\x80\x80\x80\x00\x01\x87\x80\x80\x80\x00\x00\x41\x00\x11\x01\x00\x0b");

// call_indirect.wast:522
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x04\x84\x80\x80\x80\x00\x01\x70\x00\x00\x0a\x91\x80\x80\x80\x00\x01\x8b\x80\x80\x80\x00\x00\x41\x00\x11\x94\x98\xdb\xe2\x03\x00\x0b");

// call_indirect.wast:533
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x04\x85\x80\x80\x80\x00\x01\x70\x01\x02\x02\x09\x88\x80\x80\x80\x00\x01\x00\x41\x00\x0b\x02\x00\x00");

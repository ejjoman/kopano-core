--TEST--
mapi_importhierarchychanges_importfolderchange() tests
--SKIPIF--
<?php if (!extension_loaded("mapi")) print "skip"; ?>
--FILE--
<?php
var_dump(mapi_importhierarchychanges_importfolderchange(null, null));
--EXPECTF--
Warning: mapi_importhierarchychanges_importfolderchange() expects parameter 1 to be resource, null given in %s on line %d
bool(false)

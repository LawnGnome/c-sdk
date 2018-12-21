<?php

/*DESCRIPTION
uopz_flags
*/

/*SKIPIF
<?php include("skipif.inc") ?>
*/

/*EXPECT
bool(false)
bool(true)
*/

class Foo {
	public function method() {}
}

var_dump((bool) (uopz_flags(Foo::class, "method", ZEND_ACC_PRIVATE) & ZEND_ACC_PRIVATE));
var_dump((bool) (uopz_flags(Foo::class, "method", PHP_INT_MAX) & ZEND_ACC_PRIVATE));

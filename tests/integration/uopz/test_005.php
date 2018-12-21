<?php

/*DESCRIPTION
uopz_get_mock
*/

/*SKIPIF
<?php include("skipif.inc") ?>
*/

/*EXPECT
string(3) "Bar"
object(Bar)#1 (0) {
}
*/

class Bar {}

uopz_set_mock(Foo::class, Bar::class);

var_dump(uopz_get_mock(Foo::class));

uopz_set_mock(Foo::class, new Bar);

var_dump(uopz_get_mock(Foo::class));

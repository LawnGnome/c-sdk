<?php

/*DESCRIPTION
uopz_unset_mock
*/

/*SKIPIF
<?php include("skipif.inc") ?>
*/

/*EXPECT
string(3) "Bar"
NULL
*/

class Bar {}

uopz_set_mock(Foo::class, Bar::class);

var_dump(uopz_get_mock(Foo::class));

uopz_unset_mock(Foo::class);

var_dump(uopz_get_mock(Foo::class));

<?php

if (!extension_loaded("uopz")) {
  // Attempt to load uopz.
  $result = @dl('uopz.so');
}

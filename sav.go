package main

// #include <stdlib.h>
// #include "sav_reader.h"
// #cgo windows amd64 CFLAGS: -IC:/msys64/mingw64/include
// #cgo windows LDFLAGS: -LC:/msys64/mingw64/lib -lreadstat
import "C"

import "fmt"

func main() {
	res := C.parse_sav(C.CString("staffsurvey.sav"))
	fmt.Printf("%#v\n", res)
}

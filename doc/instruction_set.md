Instruction Set
===

| Name                                  | Symbol (1 byte) | Parameters                                                                                                                          | Impl |
| noop                                  | 0x00            | None                                                                                                                                | y    |
| declare variable                      | 0x01            | 4 bytes: unsigned var index, 1 byte: type (0 = int32, 1 = link)                                                                     | y    |
| set variable                          | 0x02            | 4 bytes: unsigned var index, 1 byte: follow links, 4 bytes: content                                                                 | y    |
| undeclare variable                    | 0x03            | 4 bytes: unsigned var index                                                                                                         | y    |
| add constant to variable              | 0x04            | 4 bytes: unsigned var index, 1 byte: follow links, 4 bytes: constant to add                                                         | y    |
| add variable to variable              | 0x05            | 4 bytes: unsigned source var index, 1 byte: follow src links, 4 bytes: unsigned destination var index, 1 byte: follow dest links    | y    |
| subtract constant from variable       | 0x06            | 4 bytes: unsigned var index, 1 byte: follow links, 4 bytes: constant to subtract                                                    | y    |
| subtract variable from variable       | 0x07            | 4 bytes: unsigned source var index, 1 byte: follow src links, 4 bytes: unsigned destination var index, 1 byte: follow dest links    | y    |
| rel jump to var addr if variable > 0  | 0x08            | 4 bytes: unsigned var index, 4 bytes: rel jump address variable index; var needs to be of int type                                  | n    |
| rel jump to var addr if variable < 0  | 0x09            | 4 bytes: unsigned var index, 4 bytes: rel jump address variable index; var needs to be of int type                                  | n    |
| rel jump to var addr if variable = 0  | 0x0a            | 4 bytes: unsigned var index, 4 bytes: rel jump address variable index; var needs to be of int type                                  | n    |
| abs jump to var addr if variable > 0  | 0x0b            | 4 bytes: unsigned var index, 4 bytes: abs jump address variable index; var needs to be of int type                                  | n    |
| abs jump to var addr if variable < 0  | 0x0c            | 4 bytes: unsigned var index, 4 bytes: abs jump address variable index; var needs to be of int type                                  | n    |
| abs jump to var addr if variable = 0  | 0x0d            | 4 bytes: unsigned var index, 4 bytes: abs jump address variable index; var needs to be of int type                                  | n    |
| rel jump if variable > 0              | 0x0e            | 4 bytes: unsigned var index, 4 bytes: rel jump address                                                                              | n    |
| rel jump if variable < 0              | 0x0f            | 4 bytes: unsigned var index, 4 bytes: rel jump address                                                                              | n    |
| rel jump if variable = 0              | 0x10            | 4 bytes: unsigned var index, 4 bytes: rel jump address                                                                              | n    |
| abs jump if variable > 0              | 0x11            | 4 bytes: unsigned var index, 4 bytes: abs jump address                                                                              | n    |
| abs jump if variable < 0              | 0x12            | 4 bytes: unsigned var index, 4 bytes: abs jump address                                                                              | n    |
| abs jump if variable = 0              | 0x13            | 4 bytes: unsigned var index, 4 bytes: abs jump address                                                                              | n    |
| load memory size into variable        | 0x14            | 4 bytes: unsigned var index; var needs to be of int type                                                                            | n    |
| check if variable is input            | 0x15            | 4 bytes: unsigned var source index, 4 bytes: unsigned destination index; dest = 1 is input, 0 otherwise                             | n    |
| check if variable is output           | 0x16            | 4 bytes: unsigned var source index, 4 bytes: unsigned destination index; dest = 1 is output, 0 otherwise                            | n    |
| load input count into variable        | 0x17            | 4 bytes: unsigned var index; var needs to be of int type                                                                            | n    |
| load output count into variable       | 0x18            | 4 bytes: unsigned var index; var needs to be of int type                                                                            | n    |
| load current address into variable    | 0x19            | 4 bytes: unsigned var index; var needs to be of int type                                                                            | n    |
| print variable                        | 0x1a            | 4 bytes: unsigned var index, 1 byte: follow links                                                                                   | y    |
| set string table entry                | 0x1b            | 4 bytes: unsigned string table index, 2 bytes: length n of string, n bytes: string                                                  | y    |
| print string from string table        | 0x1c            | 4 bytes: unsigned string table index                                                                                                | y    |
| load string table limit into variable | 0x1d            | 4 bytes: unsigned var index                                                                                                         | n    |
| terminate                             | 0x1e            | 1 byte: return code                                                                                                                 | y    |
| copy variable                         | 0x1f            | 4 bytes: unsigned source var index, 1 byte: follow source links, 4 bytes: unsigned destination var index, 1 byte: follow dest links | n    |
| load string item length into variable | 0x20            | 4 bytes: string table index, 4 bytes: dest variable, 1 byte: follow                                                                 | n    |
| load string item into variables       | 0x21            | 4 bytes: string table index, 4 bytes: dest start variable, 1 byte: follow                                                           | n    |
| perform system call                   | 0x22            | 4 bytes: unsigned var idex, 1 byte: follow links, 1 byte: major code, 1 byte: minor code                                            | n    |
|                                       |                 | major code: 0  // time functions: time since start                                                                                  | n    |
|                                       |                 | * minor code: 0  // load microseconds since start into variable                                                                     | n    |
|                                       |                 | * minor code: 1  // load minutes since start into variable                                                                          | n    |
|                                       |                 | * minor code: 2  // load hours since start into variable                                                                            | n    |
|                                       |                 | * minor code: 3  // load days since start into variable                                                                             | n    |
|                                       |                 | * minor code: 4  // load months since start into variable                                                                           | n    |
|                                       |                 | * minor code: 5  // load days years start into variable                                                                             | n    |
|                                       |                 | major code: 1  // time functions: current date                                                                                      | n    |
|                                       |                 | * minor code: 0  // load current microseconds of date into variable                                                                 | n    |
|                                       |                 | * minor code: 1  // load current minute of date into variable                                                                       | n    |
|                                       |                 | * minor code: 2  // load current hour of date into variable                                                                         | n    |
|                                       |                 | * minor code: 3  // load current day of month of date into variable                                                                 | n    |
|                                       |                 | * minor code: 4  // load current month of date into variable                                                                        | n    |
|                                       |                 | * minor code: 5  // load current year of date into variable                                                                         | n    |
| bit shift variable left               | 0x23            | 4 bytes: unsigned var index, 1 byte: shift amount                                                                                   | n    |
| bit shift variable right              | 0x24            | 4 bytes: unsigned var index, 1 byte: shift amount                                                                                   | n    |
| bit-wise invert variable              | 0x25            | 4 bytes: unsigned var index                                                                                                         | n    |
| bit-wise AND two variables            | 0x26            | 4 bytes: var index 1, 1 byte: follow, 4 bytes: var index 2, 1 byte: follow, 4 bytes: var index dest, 1 byte: follow                 | n    |
| bit-wise OR two variables             | 0x27            | 4 bytes: var index 1, 1 byte: follow, 4 bytes: var index 2, 1 byte: follow, 4 bytes: var index dest, 1 byte: follow                 | n    |
| bit-wise XOR two variables            | 0x28            | 4 bytes: var index 1, 1 byte: follow, 4 bytes: var index 2, 1 byte: follow, 4 bytes: var index dest, 1 byte: follow                 | n    |
| load random value into variable       | 0x29            | 4 bytes: var index                                                                                                                  | n    |
| modulo variable by constant           | 0x30            | 4 bytes: var index, 1 byte: follow, 4 bytes: modulo value                                                                           | n    |
| modulo variable by variable           | 0x31            | 4 bytes: var index, 1 byte: follow, 4 bytes: modulo value variable, 1 byte: follow                                                  | n    |

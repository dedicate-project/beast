Instruction Set
===

| Name                                  | Symbol (1 byte) | Parameters                                                                                                                          | Impl |
| noop                                  | 0x00            | None                                                                                                                                | y    |
| declare variable                      | 0x01            | 4 bytes: unsigned var index, 1 byte: type; type: 0 = int32, 1 = link                                                                | y    |
| set variable                          | 0x02            | 4 bytes: unsigned var index, 1 byte: follow links, 4 bytes: content, based on declared type                                         | n    |
| undeclare variable                    | 0x03            | 4 bytes: unsigned var index                                                                                                         | n    |
| add constant to variable              | 0x04            | 4 bytes: unsigned source var index, 4 bytes: unsigned destination var index, 4 bytes: constant to add, based on declared type       | n    |
| add variable to variable              | 0x05            | 4 bytes: unsigned source var index, 4 bytes: unsigned destination var index, 4 bytes: var index to add, based on declared type      | n    |
| subtract constant from variable       | 0x06            | 4 bytes: unsigned source var index, 4 bytes: unsigned destination var index, 4 bytes: constant to subtract, based on declared type  | n    |
| subtract variable from variable       | 0x07            | 4 bytes: unsigned source var index, 4 bytes: unsigned destination var index, 4 bytes: var index to subtract, based on declared type | n    |
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
| print variable                        | 0x1a            | 4 bytes: unsigned var index                                                                                                         | n    |
| set string table entry                | 0x1b            | 4 bytes: unsigned string table index, 2 bytes: length n of string, n bytes: string                                                  | n    |
| print string from string table        | 0x1c            | 4 bytes: unsigned string table index                                                                                                | n    |
| load string table limit into variable | 0x1d            | 4 bytes: unsigned var index                                                                                                         | n    |
| terminate                             | 0x1e            | 1 byte: return code                                                                                                                 | n    |
| copy variable                         | 0x1f            | 4 bytes: unsigned source var index, 4 bytes: unsigned destination var index                                                         | n    |

# Functions
1. Lift x86 instructions to IR(intermediate representation) (SSA Form Based)
2. IR Optimization
3. Dynamic symbolic analysis with IR

# IR Optimization
## Dead Store Elimination

- Example<br>
(1) VMProtect Mutation Code<br>
![image](https://github.com/DoubleS1405/x86_Optimizing/assets/15829327/ad1eb2fd-44e9-40b7-bb43-a50554cd8efb)<br><br>
(2) Optimized Code <br>
![image](https://github.com/DoubleS1405/x86_Optimizing/assets/15829327/691a1703-4d8c-40d1-88f4-3089e73827e0)<br><br>
<br>

# Dynamic symbolic analysis with IR
## Dead Store Elimination

(1) First Char<br>
![image](https://github.com/DoubleS1405/x88Solve_x64dbgPlugin/assets/15829327/52413d62-f633-4656-bdda-57e205318a5b) <br><br>

0x71 -> q  <br>

(2) Second Char<br>

![image](https://github.com/DoubleS1405/x88Solve_x64dbgPlugin/assets/15829327/e9ab8eed-a020-4e8c-86f0-b4c79dd06128) <br><br>

0x65 -> e  <br>

(3) Third Char<br>

![image](https://github.com/DoubleS1405/x88Solve_x64dbgPlugin/assets/15829327/0176a0f2-3654-401d-aa5f-7d323a153656) <br><br>

0x7a->z  <br>

(4) Fourth Char<br>

![image](https://github.com/DoubleS1405/x88Solve_x64dbgPlugin/assets/15829327/b9f55688-5754-4830-a86e-2c3e8d395c1c) <br><br>

0x6a->j  <br>

(5) Fifth Char<br>

![image](https://github.com/DoubleS1405/x88Solve_x64dbgPlugin/assets/15829327/6f2e5e14-1467-4697-9f25-cc8e991b86be) <br><br>

0x64->d  <br>
<br>
<br>
....
<br>
<br>

(6) Last Char <br>

![image](https://github.com/DoubleS1405/x88Solve_x64dbgPlugin/assets/15829327/c01ce384-6970-4a0c-8892-fb4772b5e868) <br><br>

0x76->v <br>


(7) Flag <br>

![image](https://github.com/DoubleS1405/x88Solve_x64dbgPlugin/assets/15829327/769baa6f-8dec-4741-afce-9ec01734039f) <br><br>


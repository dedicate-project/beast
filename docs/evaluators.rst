Evaluators
==========

Programs can be manually written or automatically generated to fulfill a certain purpose, or to
follow specific criteria. This applies to both, static structure and runtime behavior. To be able to
judge programs based on generic or specific criteria, Evaluators can be used.

Evaluators take a `VmSession` of an already executed program as input and inspect its runtime
statistics as well as the associated program's structure. Their application can vary from
generically judging how efficient a program is executed (e.g., how much of the present code is
actually executed), how much of the code (static or executed) are `OpCode::NoOp` operators, to more
sophisticated checks like how well does a program sort a given array of numbers or process any other
kind of input.

Currently, three generic classes of Evaluators are contained with the BEAST library:

* ``Aggregation Evaluator``: Aggregates the (weighted and optionally inverted) score values of one
  or more evaluators

* ``Operator Usage Evaluator``: Determines how many times a specific operator was used

* ``Runtime Statistics Evaluator``: Evaluates the static structure and dynamic behavior of a program

More specific evaluators will follow on a per-need basis. In any case, all evaluators need to derive
from the `beast::Evaluator` class and adhere to its interface.

.. doxygenclass:: beast::Evaluator
   :members:


Aggregation Evaluator
---------------------

Allows to aggregate the score value of one or more arbitrary evaluators. In addition to that,
evaluator scores can be weighted relatively, and their logic can optionally be inverted (1.0 becomes
0.0 and vice versa). This evaluator is useful to combine multiple independent criteria, and produce an aggregated value that reflects the exact criteria a program should be scored by.

.. doxygenclass:: beast::AggregationEvaluator
   :members:


Operator Usage Evaluator
------------------------

This evaluator checks how many of the executed operators in a program have been of a specific type
and divides that number by the total number of steps executed. This results in a score from `0.0`
(no operators of the given type) to `1.0` (only those operators). Depending on the use-case,
emphasize may be put on different extremes of this score (invert the logic accordingly using an
`AggregationEvaluator` if required).

.. doxygenclass:: beast::OperatorUsageEvaluator
   :members:


Runtime Statistics Evaluator
----------------------------

This evaluator inspects the dynamic runtime behavior of an executed program and its static
structure. In order to get to a good measure of efficiency in both, static program structure AND
dynamic runtime behavior, three things must be met:

* As few operators executed as possible should be noop operators. This means that we don't waste
  time on empty cycles but execute efficiently. The variable holding a measure of this is
  steps_executed_noop_fraction.

* As many operators actually present in the program code as possible should be noop operators. This
  means that the program's function can be executed with a slender static structure rather than
  bloated code. The variable holding a measure of this is total_steps_noop_fraction.

* As few individual operators as possible should actually be executed. This means that a program
  makes good use of iterations and jumps, and doesn't rely on overfitting to any particular
  task. The variable holding a measure of this is program_executed_fraction.

These aspects need to be combined into a single score value. Given the considerations from above,
the formula for this score value hence becomes:

.. code-block:: cpp

   prg_exec_weight = 1.0 - dyn_noop_weight - stat_noop_weight
                     with dyn_noop_weight + stat_noop_weight <= 1.0

   score =
       dyn_noop_weight * (1.0 - steps_executed_noop_fraction) +
       stat_noop_weight * total_steps_noop_fraction +
       prg_exec_weight * (1.0 - program_executed_fraction)

The weight values `dyn_noop_weight` and `stat_noop_weight` are initialization parameters to the
evaluator and must be chosen carefully so that programs can correctly converge to a good balance
between runtime and static structure efficiency.

.. doxygenclass:: beast::RuntimeStatisticsEvaluator
   :members:

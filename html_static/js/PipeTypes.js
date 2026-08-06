// Single source of truth for the pipe types the compose UI can author. Each entry knows
// how to render its own parameter form (`fields`), how to translate that form's values
// back into the on-wire JSON shape the backend expects (`buildParameters`), and how to
// describe itself visually on the pipeline canvas (`image`, `inputs`, `outputs`).
//
// Backed by the corresponding C++ branches of `PipelineManager::constructPipelineFromJson`
// in src/pipeline_manager.cpp; if you add a pipe type here you'll almost certainly want to
// extend that switch too (or the backend will reject the add_pipe action).

// Helper used by the EvaluatorPipe definition; pulled out so the form code can stay flat
// instead of growing one nested ternary per knob.
const evolutionParameterDefaults = {
  generations: 50,
  crossover_probability: 0.7,
  mutation_probability: 0.05,
  elitism: true,
  byte_mutation_share: 0.2,
  variable_count: 16,
  string_table_size: 4,
  string_table_item_length: 16,
  max_genome_bytes: 2048,
  starting_program_size: 0,
  opcode_weights: {},
};

export const PIPE_TYPE_DEFINITIONS = [
  {
    type: 'ProgramFactoryPipe',
    label: 'Program Factory',
    description: 'Emits random candidate programs for downstream pipes to evolve.',
    image: '/img/factory_pipe.png',
    inputs: 0,
    outputs: 1,
    sections: [
      {
        title: 'Factory',
        fields: [
          {
            name: 'factory',
            label: 'Program factory',
            type: 'enum',
            options: [{value: 'RandomProgramFactory', label: 'Random programs'}],
            default: 'RandomProgramFactory',
          },
          {name: 'max_candidates', label: 'Max candidates per cycle', type: 'int', default: 50, min: 1},
          {name: 'max_size', label: 'Max program size (bytes)', type: 'int', default: 80, min: 1},
        ],
      },
      {
        title: 'VM session',
        fields: [
          {name: 'memory_variables', label: 'Memory variables', type: 'int', default: 64, min: 1},
          {name: 'string_table_items', label: 'String table entries', type: 'int', default: 0, min: 0},
          {name: 'string_table_item_length', label: 'String table item length', type: 'int', default: 0, min: 0},
        ],
      },
    ],
    buildParameters: (values) => ({
      factory: values.factory,
      max_candidates: values.max_candidates,
      max_size: values.max_size,
      memory_variables: values.memory_variables,
      string_table_items: values.string_table_items,
      string_table_item_length: values.string_table_item_length,
    }),
    parseParameters: (params) => ({
      factory: params.factory,
      max_candidates: params.max_candidates,
      max_size: params.max_size,
      memory_variables: params.memory_variables,
      string_table_items: params.string_table_items,
      string_table_item_length: params.string_table_item_length,
    }),
  },
  {
    type: 'MazeEvaluatorPipe',
    label: 'Maze Evaluator',
    description: 'Wraps an EvaluatorPipe around a MazeEvaluator to evolve programs against a procedurally generated maze.',
    // What the backend actually sees: an EvaluatorPipe with a single MazeEvaluator child.
    // `buildAsType` is used by AddPipeDialog when POSTing; the canvas keeps using the
    // EvaluatorPipe-with-MazeEvaluator detection logic that's already in PipelineCanvas.js
    // to pick the maze icon for incoming pipes.
    buildAsType: 'EvaluatorPipe',
    image: '/img/maze_evaluator_pipe.png',
    inputs: 1,
    outputs: 1,
    sections: [
      {
        title: 'Pipe',
        fields: [
          {name: 'max_candidates', label: 'Max candidates per cycle', type: 'int', default: 20, min: 1},
          {name: 'memory_variables', label: 'Memory variables', type: 'int', default: 64, min: 1},
          {name: 'string_table_items', label: 'String table entries', type: 'int', default: 0, min: 0},
          {name: 'string_table_item_length', label: 'String table item length', type: 'int', default: 0, min: 0},
        ],
      },
      {
        title: 'Maze',
        fields: [
          {name: 'rows', label: 'Rows', type: 'int', default: 8, min: 3},
          {name: 'cols', label: 'Columns', type: 'int', default: 8, min: 3},
          {name: 'difficulty', label: 'Difficulty (0-1)', type: 'float', default: 0.1, min: 0.0, max: 1.0, step: 0.05},
          {name: 'max_steps', label: 'Max VM steps per evaluation', type: 'int', default: 2000, min: 1},
        ],
      },
      {
        title: 'Selection',
        fields: [
          {name: 'cut_off_score', label: 'Cut-off score', type: 'float', default: 0.0, min: 0.0, max: 1.0, step: 0.01},
        ],
      },
      {
        // See the matching note in `numericEvaluatorPipe` for the fallback contract.
        // The MazeEvaluator currently has no GPU port, so picking "gpu" here will fall
        // back to CPU; the option is exposed for parity with the other evaluator pipes
        // and so saved pipelines round-trip cleanly when a Maze pipe is dropped on a
        // CUDA-enabled binary.
        title: 'Execution backend',
        collapsedByDefault: true,
        fields: [
          {
            name: 'backend', label: 'Backend', type: 'enum',
            options: [
              {value: 'cpu',  label: 'CPU (default, always available)'},
              {value: 'gpu',  label: 'GPU (CUDA; falls back to CPU if unavailable)'},
              {value: 'auto', label: 'Auto (GPU when applicable, otherwise CPU)'},
            ],
            default: 'cpu',
          },
        ],
      },
      {
        title: 'Genetic algorithm',
        // Collapsed by default since the defaults are usually fine; the dialog renders
        // sections as collapsible accordions.
        collapsedByDefault: true,
        fields: [
          {name: 'generations', label: 'Generations per cycle', type: 'int', default: evolutionParameterDefaults.generations, min: 1},
          {name: 'crossover_probability', label: 'Crossover probability', type: 'float', default: evolutionParameterDefaults.crossover_probability, min: 0.0, max: 1.0, step: 0.05},
          {name: 'mutation_probability', label: 'Mutation probability', type: 'float', default: evolutionParameterDefaults.mutation_probability, min: 0.0, max: 1.0, step: 0.01},
          {name: 'byte_mutation_share', label: 'Byte-level mutation share', type: 'float', default: evolutionParameterDefaults.byte_mutation_share, min: 0.0, max: 1.0, step: 0.05},
          {name: 'elitism', label: 'Elitism (keep best each gen)', type: 'bool', default: evolutionParameterDefaults.elitism},
          {name: 'starting_program_size', label: 'Starting program size (0 = factory default)', type: 'int', default: evolutionParameterDefaults.starting_program_size, min: 0},
          {name: 'max_genome_bytes', label: 'Max genome size (bytes)', type: 'int', default: evolutionParameterDefaults.max_genome_bytes, min: 1},
        ],
      },
    ],
    buildParameters: (values) => ({
      max_candidates: values.max_candidates,
      memory_variables: values.memory_variables,
      string_table_items: values.string_table_items,
      string_table_item_length: values.string_table_item_length,
      cut_off_score: values.cut_off_score,
      // Backend key elided when CPU (the default) so legacy pipelines round-trip
      // through Load -> Save without sprouting a no-op `backend: "cpu"` field.
      ...(values.backend && values.backend !== 'cpu' ? {backend: values.backend} : {}),
      evaluators: [{
        type: 'MazeEvaluator',
        weight: 1.0,
        invert_logic: false,
        parameters: {
          rows: values.rows,
          cols: values.cols,
          difficulty: values.difficulty,
          max_steps: values.max_steps,
        },
      }],
      evolution_parameters: {
        ...evolutionParameterDefaults,
        generations: values.generations,
        crossover_probability: values.crossover_probability,
        mutation_probability: values.mutation_probability,
        byte_mutation_share: values.byte_mutation_share,
        elitism: values.elitism,
        starting_program_size: values.starting_program_size,
        max_genome_bytes: values.max_genome_bytes,
      },
    }),
    // The inverse of buildParameters; used by the EditPipeDialog when populating the form
    // from an existing pipe's JSON. Missing fields fall back to the schema defaults so an
    // older on-disk pipe that predates a knob still opens cleanly.
    parseParameters: (params) => {
      const maze = (params.evaluators && params.evaluators[0] && params.evaluators[0].parameters) || {};
      const ga = params.evolution_parameters || {};
      return {
        max_candidates: params.max_candidates,
        memory_variables: params.memory_variables,
        string_table_items: params.string_table_items,
        string_table_item_length: params.string_table_item_length,
        rows: maze.rows,
        cols: maze.cols,
        difficulty: maze.difficulty,
        max_steps: maze.max_steps,
        cut_off_score: params.cut_off_score,
        // Older Maze pipelines predate the backend selector; missing -> CPU so the
        // form populates with a concrete option rather than an empty dropdown.
        backend: params.backend || 'cpu',
        generations: ga.generations,
        crossover_probability: ga.crossover_probability,
        mutation_probability: ga.mutation_probability,
        byte_mutation_share: ga.byte_mutation_share,
        elitism: ga.elitism,
        starting_program_size: ga.starting_program_size,
        max_genome_bytes: ga.max_genome_bytes,
      };
    },
  },
  // Helper that emits a {buildParameters, parseParameters, sections} bundle shared by the
  // numeric-task evaluator pipes (Adder, Maximum, ...). Each one is an EvaluatorPipe whose
  // child evaluator differs only in its parameters, so factoring out the EvaluatorPipe
  // chrome keeps the per-task definition focused on its own knobs.
  ...(() => {
    const numericEvaluatorPipe = ({
      type, label, description, image, evaluatorType, extraEvaluatorFields, evaluatorBuild,
      evaluatorParse, defaultOutputCount = 1,
    }) => ({
      type,
      label,
      description,
      buildAsType: 'EvaluatorPipe',
      image,
      inputs: 1,
      outputs: defaultOutputCount,
      sections: [
        {
          title: 'Pipe',
          fields: [
            {name: 'max_candidates', label: 'Max candidates per cycle', type: 'int', default: 20, min: 1},
            {name: 'memory_variables', label: 'Memory variables', type: 'int', default: 32, min: 1},
            {name: 'string_table_items', label: 'String table entries', type: 'int', default: 0, min: 0},
            {name: 'string_table_item_length', label: 'String table item length', type: 'int', default: 0, min: 0},
          ],
        },
        {
          title: 'Task',
          fields: extraEvaluatorFields,
        },
        {
          title: 'Selection',
          fields: [
            {name: 'cut_off_score', label: 'Cut-off score', type: 'float', default: 0.0, min: 0.0, max: 1.0, step: 0.01},
          ],
        },
        {
          // Backend dispatch is plumbed all the way to the C++ EvolutionPipe via
          // EvaluatorPipe::applyBackendSelection. "gpu" / "auto" only actually use the
          // GPU when (a) the binary was built with BEAST_ENABLE_CUDA, (b) a CUDA device
          // is visible, and (c) the evaluator has a CUDA port (today only SHA-256). All
          // other combinations silently fall back to the CPU thread pool -- pipelines
          // stay portable between GPU and CPU-only hosts.
          title: 'Execution backend',
          collapsedByDefault: true,
          fields: [
            {
              name: 'backend', label: 'Backend', type: 'enum',
              options: [
                {value: 'cpu',  label: 'CPU (default, always available)'},
                {value: 'gpu',  label: 'GPU (CUDA; falls back to CPU if unavailable)'},
                {value: 'auto', label: 'Auto (GPU when applicable, otherwise CPU)'},
              ],
              default: 'cpu',
            },
          ],
        },
        {
          title: 'Genetic algorithm',
          collapsedByDefault: true,
          fields: [
            {name: 'generations', label: 'Generations per cycle', type: 'int', default: evolutionParameterDefaults.generations, min: 1},
            {name: 'crossover_probability', label: 'Crossover probability', type: 'float', default: evolutionParameterDefaults.crossover_probability, min: 0.0, max: 1.0, step: 0.05},
            {name: 'mutation_probability', label: 'Mutation probability', type: 'float', default: evolutionParameterDefaults.mutation_probability, min: 0.0, max: 1.0, step: 0.01},
            {name: 'byte_mutation_share', label: 'Byte-level mutation share', type: 'float', default: evolutionParameterDefaults.byte_mutation_share, min: 0.0, max: 1.0, step: 0.05},
            {name: 'elitism', label: 'Elitism (keep best each gen)', type: 'bool', default: evolutionParameterDefaults.elitism},
            {name: 'starting_program_size', label: 'Starting program size (0 = factory default)', type: 'int', default: evolutionParameterDefaults.starting_program_size, min: 0},
            {name: 'max_genome_bytes', label: 'Max genome size (bytes)', type: 'int', default: evolutionParameterDefaults.max_genome_bytes, min: 1},
          ],
        },
        {
          // The library is rebuilt at the start of every cycle from the configured
          // ledger paths, so changes here take effect on the next cycle without
          // restarting the pipeline. Each entry adds one or more subroutines to the
          // shared library indexed by ascending `subroutine_id`.
          title: 'Subroutines (advanced)',
          collapsedByDefault: true,
          fields: [
            {
              name: 'subroutines',
              label: 'Subroutine sources (JSON array)',
              type: 'json',
              minRows: 6,
              default: '[]',
              placeholder:
                'JSON array of {ledger_path, top_k, input_arity, output_arity, max_steps_per_call}',
            },
          ],
        },
      ],
      buildParameters: (values) => {
        // Subroutines field is a raw JSON string; parse defensively so a typo in the
        // form doesn't drop the whole pipe -- the backend will surface a precise error
        // if the parsed array is malformed.
        let subroutines = [];
        try {
          const parsed = JSON.parse(values.subroutines || '[]');
          if (Array.isArray(parsed)) {
            subroutines = parsed;
          }
        } catch (err) {
          subroutines = [];
        }
        const params = {
          max_candidates: values.max_candidates,
          memory_variables: values.memory_variables,
          string_table_items: values.string_table_items,
          string_table_item_length: values.string_table_item_length,
          cut_off_score: values.cut_off_score,
          // Only emit the backend key when non-default, mirroring the C++ serializer:
          // legacy pipelines round-trip without sprouting a `backend: "cpu"` key.
          ...(values.backend && values.backend !== 'cpu' ? {backend: values.backend} : {}),
          evaluators: [{
            type: evaluatorType,
            weight: 1.0,
            invert_logic: false,
            parameters: evaluatorBuild(values),
          }],
          evolution_parameters: {
            ...evolutionParameterDefaults,
            generations: values.generations,
            crossover_probability: values.crossover_probability,
            mutation_probability: values.mutation_probability,
            byte_mutation_share: values.byte_mutation_share,
            elitism: values.elitism,
            starting_program_size: values.starting_program_size,
            max_genome_bytes: values.max_genome_bytes,
          },
        };
        if (subroutines.length > 0) {
          params.subroutines = subroutines;
        }
        return params;
      },
      parseParameters: (params) => {
        const task = (params.evaluators && params.evaluators[0] && params.evaluators[0].parameters) || {};
        const ga = params.evolution_parameters || {};
        return {
          max_candidates: params.max_candidates,
          memory_variables: params.memory_variables,
          string_table_items: params.string_table_items,
          string_table_item_length: params.string_table_item_length,
          cut_off_score: params.cut_off_score,
          // Missing `backend` in the on-wire form means CPU (the historical default);
          // keep that mapping explicit so the form always renders with a selection.
          backend: params.backend || 'cpu',
          ...evaluatorParse(task),
          generations: ga.generations,
          crossover_probability: ga.crossover_probability,
          mutation_probability: ga.mutation_probability,
          byte_mutation_share: ga.byte_mutation_share,
          elitism: ga.elitism,
          starting_program_size: ga.starting_program_size,
          max_genome_bytes: ga.max_genome_bytes,
          // Pretty-print for editability when we round-trip back into the form.
          subroutines: params.subroutines
            ? JSON.stringify(params.subroutines, null, 2)
            : '[]',
        };
      },
    });
    return [
      numericEvaluatorPipe({
        type: 'AdderEvaluatorPipe',
        label: 'Adder Evaluator',
        description:
          'Evolves programs that compute a + b. Each evaluation runs N random pairs ' +
          'through the candidate program and averages the per-trial score (1.0 = exact, ' +
          'smooth decay with absolute error).',
        image: '/img/adder_evaluator_pipe.png',
        evaluatorType: 'AdderEvaluator',
        extraEvaluatorFields: [
          {name: 'trial_count', label: 'Trials per evaluation', type: 'int', default: 8, min: 1, max: 64},
          {name: 'value_range', label: 'Input range (±)', type: 'int', default: 32, min: 1},
          {name: 'max_steps_per_trial', label: 'VM steps per trial', type: 'int', default: 800, min: 1},
        ],
        evaluatorBuild: (v) => ({
          trial_count: v.trial_count,
          value_range: v.value_range,
          max_steps_per_trial: v.max_steps_per_trial,
        }),
        evaluatorParse: (p) => ({
          trial_count: p.trial_count,
          value_range: p.value_range,
          max_steps_per_trial: p.max_steps_per_trial,
        }),
      }),
      numericEvaluatorPipe({
        type: 'MaximumEvaluatorPipe',
        label: 'Maximum Evaluator',
        description:
          'Evolves programs that return the maximum of N integer inputs. Tougher than ' +
          'addition because it requires the program to compare values; useful for ' +
          'curriculum learning once a population can already handle Adder.',
        image: '/img/maximum_evaluator_pipe.png',
        evaluatorType: 'MaximumEvaluator',
        extraEvaluatorFields: [
          {name: 'input_count', label: 'Inputs to compare (2-16)', type: 'int', default: 3, min: 2, max: 16},
          {name: 'trial_count', label: 'Trials per evaluation', type: 'int', default: 8, min: 1, max: 64},
          {name: 'value_range', label: 'Input range (±)', type: 'int', default: 64, min: 1},
          {name: 'max_steps_per_trial', label: 'VM steps per trial', type: 'int', default: 1200, min: 1},
        ],
        evaluatorBuild: (v) => ({
          input_count: v.input_count,
          trial_count: v.trial_count,
          value_range: v.value_range,
          max_steps_per_trial: v.max_steps_per_trial,
        }),
        evaluatorParse: (p) => ({
          input_count: p.input_count,
          trial_count: p.trial_count,
          value_range: p.value_range,
          max_steps_per_trial: p.max_steps_per_trial,
        }),
      }),
      numericEvaluatorPipe({
        type: 'IdentityEvaluatorPipe',
        label: 'Identity Evaluator',
        description:
          'Curriculum stage 0: copy N input words verbatim to N output words. The ' +
          'trivial first lesson -- proves the genome can address every output slot. ' +
          'Survivors here make excellent seed material for downstream stages (rotate, ' +
          'sigma, ch, maj, ...) via a ProgramStorageSink + Source loop.',
        image: '/img/identity_evaluator_pipe.png',
        evaluatorType: 'IdentityEvaluator',
        extraEvaluatorFields: [
          {name: 'trial_count', label: 'Trials per evaluation', type: 'int', default: 8, min: 1, max: 64},
          {name: 'width', label: 'Words to copy (1-16)', type: 'int', default: 8, min: 1, max: 16},
          {name: 'max_steps_per_trial', label: 'VM steps per trial', type: 'int', default: 1500, min: 1},
        ],
        evaluatorBuild: (v) => ({
          trial_count: v.trial_count, width: v.width, max_steps_per_trial: v.max_steps_per_trial,
        }),
        evaluatorParse: (p) => ({
          trial_count: p.trial_count, width: p.width, max_steps_per_trial: p.max_steps_per_trial,
        }),
      }),
      numericEvaluatorPipe({
        type: 'BitwiseEvaluatorPipe',
        label: 'Bitwise Evaluator',
        description:
          'Curriculum stage that scores programs on computing one specific bitwise ' +
          'operation (XOR / AND / OR for two inputs at vars 0,1; NOT for one input at ' +
          'var 0). Use to grow a population that\'s fluent in the bit primitives the ' +
          'SHA-256 round actually needs.',
        image: '/img/bitwise_evaluator_pipe.png',
        evaluatorType: 'BitwiseEvaluator',
        extraEvaluatorFields: [
          {name: 'trial_count', label: 'Trials per evaluation', type: 'int', default: 8, min: 1, max: 64},
          {
            name: 'operation', label: 'Operation', type: 'enum',
            options: [
              {value: 'xor', label: 'XOR (a ^ b)'},
              {value: 'and', label: 'AND (a & b)'},
              {value: 'or',  label: 'OR (a | b)'},
              {value: 'not', label: 'NOT (~a)'},
            ],
            default: 'xor',
          },
          {name: 'max_steps_per_trial', label: 'VM steps per trial', type: 'int', default: 1500, min: 1},
        ],
        evaluatorBuild: (v) => ({
          trial_count: v.trial_count, operation: v.operation,
          max_steps_per_trial: v.max_steps_per_trial,
        }),
        evaluatorParse: (p) => ({
          trial_count: p.trial_count, operation: p.operation || 'xor',
          max_steps_per_trial: p.max_steps_per_trial,
        }),
      }),
      numericEvaluatorPipe({
        type: 'RotateEvaluatorPipe',
        label: 'Rotate Evaluator',
        description:
          'Curriculum stage that scores programs on right-rotating (or left-rotating) ' +
          'a single input word by a fixed amount. SHA-256 uses ten distinct rotation ' +
          'amounts (2, 6, 7, 11, 13, 17, 18, 19, 22, 25); instantiate one rotate pipe ' +
          'per amount to teach each as a separate skill, then merge the surviving ' +
          'populations into the Sigma / round stages downstream.',
        image: '/img/rotate_evaluator_pipe.png',
        evaluatorType: 'RotateEvaluator',
        extraEvaluatorFields: [
          {name: 'trial_count', label: 'Trials per evaluation', type: 'int', default: 8, min: 1, max: 64},
          {name: 'amount', label: 'Rotation amount (1-31)', type: 'int', default: 2, min: 1, max: 31},
          {
            name: 'direction', label: 'Direction', type: 'enum',
            options: [
              {value: 'right', label: 'Right (rotr)'},
              {value: 'left', label: 'Left (rotl)'},
            ],
            default: 'right',
          },
          {name: 'max_steps_per_trial', label: 'VM steps per trial', type: 'int', default: 1200, min: 1},
        ],
        evaluatorBuild: (v) => ({
          trial_count: v.trial_count, amount: v.amount, direction: v.direction,
          max_steps_per_trial: v.max_steps_per_trial,
        }),
        evaluatorParse: (p) => ({
          trial_count: p.trial_count, amount: p.amount, direction: p.direction || 'right',
          max_steps_per_trial: p.max_steps_per_trial,
        }),
      }),
      numericEvaluatorPipe({
        type: 'Sha256SigmaEvaluatorPipe',
        label: 'SHA-256 Sigma Evaluator',
        description:
          'Curriculum stage targeting one of the four SHA-256 sigma functions ' +
          '(BigSigma0/1 used inside the round body, SmallSigma0/1 used inside the ' +
          'message-schedule expansion). Each variant is the XOR of 2-3 rotations / ' +
          'shifts -- the smallest meaningful composite of the rotate + bitwise ' +
          'primitives. A great waypoint between the per-skill stages and the full ' +
          'round.',
        image: '/img/sha256_sigma_evaluator_pipe.png',
        evaluatorType: 'Sha256SigmaEvaluator',
        extraEvaluatorFields: [
          {name: 'trial_count', label: 'Trials per evaluation', type: 'int', default: 8, min: 1, max: 64},
          {
            name: 'variant', label: 'Sigma variant', type: 'enum',
            options: [
              {value: 'big0',   label: 'BigSigma0  (rotr2 ^ rotr13 ^ rotr22)'},
              {value: 'big1',   label: 'BigSigma1  (rotr6 ^ rotr11 ^ rotr25)'},
              {value: 'small0', label: 'SmallSigma0 (rotr7 ^ rotr18 ^ shr3)'},
              {value: 'small1', label: 'SmallSigma1 (rotr17 ^ rotr19 ^ shr10)'},
            ],
            default: 'big0',
          },
          {name: 'max_steps_per_trial', label: 'VM steps per trial', type: 'int', default: 2000, min: 1},
        ],
        evaluatorBuild: (v) => ({
          trial_count: v.trial_count, variant: v.variant,
          max_steps_per_trial: v.max_steps_per_trial,
        }),
        evaluatorParse: (p) => ({
          trial_count: p.trial_count, variant: p.variant || 'big0',
          max_steps_per_trial: p.max_steps_per_trial,
        }),
      }),
      numericEvaluatorPipe({
        type: 'Sha256ChEvaluatorPipe',
        label: 'SHA-256 Ch Evaluator',
        description:
          'Curriculum stage for the SHA-256 "choose" function: Ch(x, y, z) = ' +
          '(x AND y) XOR ((NOT x) AND z). Three inputs (vars 0, 1, 2), one output ' +
          '(var 4). The reference function is 4 bitwise opcodes -- one of the more ' +
          'compact building blocks of the round body.',
        image: '/img/sha256_ch_evaluator_pipe.png',
        evaluatorType: 'Sha256ChEvaluator',
        extraEvaluatorFields: [
          {name: 'trial_count', label: 'Trials per evaluation', type: 'int', default: 8, min: 1, max: 64},
          {name: 'max_steps_per_trial', label: 'VM steps per trial', type: 'int', default: 1500, min: 1},
        ],
        evaluatorBuild: (v) => ({
          trial_count: v.trial_count, max_steps_per_trial: v.max_steps_per_trial,
        }),
        evaluatorParse: (p) => ({
          trial_count: p.trial_count, max_steps_per_trial: p.max_steps_per_trial,
        }),
      }),
      numericEvaluatorPipe({
        type: 'Sha256MajEvaluatorPipe',
        label: 'SHA-256 Maj Evaluator',
        description:
          'Curriculum stage for the SHA-256 "majority" function: Maj(x, y, z) = ' +
          '(x AND y) XOR (x AND z) XOR (y AND z). Three inputs (vars 0, 1, 2), one ' +
          'output (var 4). Five bitwise opcodes in BEAST -- a sibling to Ch.',
        image: '/img/sha256_maj_evaluator_pipe.png',
        evaluatorType: 'Sha256MajEvaluator',
        extraEvaluatorFields: [
          {name: 'trial_count', label: 'Trials per evaluation', type: 'int', default: 8, min: 1, max: 64},
          {name: 'max_steps_per_trial', label: 'VM steps per trial', type: 'int', default: 1500, min: 1},
        ],
        evaluatorBuild: (v) => ({
          trial_count: v.trial_count, max_steps_per_trial: v.max_steps_per_trial,
        }),
        evaluatorParse: (p) => ({
          trial_count: p.trial_count, max_steps_per_trial: p.max_steps_per_trial,
        }),
      }),
      numericEvaluatorPipe({
        type: 'Sha256RoundEvaluatorPipe',
        label: 'SHA-256 Round Evaluator',
        description:
          'Evolves programs that reproduce one round of the SHA-256 compression ' +
          'function. Reads working state (a..h) from vars 0..7, K from var 8 and W ' +
          'from var 9; writes the new (a..h) to vars 11..18. Scored bit-by-bit ' +
          '(Hamming distance) so the GA has a smooth gradient to climb -- pure ' +
          'exact-match scoring would degenerate into needle-in-a-haystack hash ' +
          'inversion. Trials per evaluation control how many distinct (state, K, W) ' +
          'patterns the candidate is scored on -- raise it (and pick a non-fixed ' +
          'round-constants mode) to stop the GA from memorising a small lookup table ' +
          'instead of learning the round formula. First rung of a curriculum that ' +
          'eventually composes the schedule, block compression and padding into a ' +
          'full SHA-256 hasher.',
        image: '/img/sha256_round_evaluator_pipe.png',
        evaluatorType: 'Sha256RoundEvaluator',
        extraEvaluatorFields: [
          {name: 'trial_count', label: 'Trials per evaluation', type: 'int', default: 16, min: 1, max: 256},
          {name: 'round_constant_index', label: 'SHA-256 round index / start offset (0-63)', type: 'int',
           default: 0, min: 0, max: 63},
          {
            name: 'round_constants_mode', label: 'Round constants mode', type: 'enum',
            options: [
              {value: 'fixed',            label: 'Fixed (single K, easiest to memorise)'},
              {value: 'cycle_all',        label: 'Cycle all (walk K table from start offset)'},
              {value: 'random_per_trial', label: 'Random per trial (deterministic but varied)'},
            ],
            default: 'fixed',
          },
          {name: 'rounds_per_trial', label: 'Rounds per trial (program loops internally)', type: 'int',
           default: 1, min: 1, max: 64},
          {name: 'max_steps_per_trial', label: 'VM steps per round (x rounds = trial budget)', type: 'int',
           default: 4000, min: 1},
        ],
        evaluatorBuild: (v) => ({
          trial_count: v.trial_count,
          round_constant_index: v.round_constant_index,
          round_constants_mode: v.round_constants_mode,
          rounds_per_trial: v.rounds_per_trial,
          max_steps_per_trial: v.max_steps_per_trial,
        }),
        evaluatorParse: (p) => ({
          trial_count: p.trial_count,
          round_constant_index: p.round_constant_index,
          round_constants_mode: p.round_constants_mode || 'fixed',
          rounds_per_trial: p.rounds_per_trial != null ? p.rounds_per_trial : 1,
          max_steps_per_trial: p.max_steps_per_trial,
        }),
      }),
      numericEvaluatorPipe({
        type: 'PopcountEvaluatorPipe',
        label: 'Popcount Evaluator',
        description:
          'Primitive gym: count the set bits in a single 32-bit input word. Solvable in ' +
          'a few opcodes with the classic Hacker\'s Delight trick. Scoring uses numeric ' +
          'distance (the output is 0..32, not a bitfield, so bit-Hamming would lie about ' +
          'how close the candidate is). Survivors make a small, fast subroutine that ' +
          'downstream consumers can mount to count features in any input word.',
        image: '/img/popcount_evaluator_pipe.png',
        evaluatorType: 'PopcountEvaluator',
        extraEvaluatorFields: [
          {name: 'trial_count', label: 'Trials per evaluation', type: 'int', default: 8, min: 1, max: 64},
          {name: 'max_steps_per_trial', label: 'VM steps per trial', type: 'int', default: 800, min: 1},
        ],
        evaluatorBuild: (v) => ({
          trial_count: v.trial_count, max_steps_per_trial: v.max_steps_per_trial,
        }),
        evaluatorParse: (p) => ({
          trial_count: p.trial_count, max_steps_per_trial: p.max_steps_per_trial,
        }),
      }),
      numericEvaluatorPipe({
        type: 'ParityEvaluatorPipe',
        label: 'Parity Evaluator',
        description:
          'Primitive gym: XOR-reduce N input words (default 4) to one output word. The ' +
          'easiest meaningful target in the whole suite -- three XOR opcodes plus the ' +
          'load/store overhead. A useful "is everything wired up correctly" smoke test ' +
          'and a tiny subroutine for downstream signature/digest features.',
        image: '/img/parity_evaluator_pipe.png',
        evaluatorType: 'ParityEvaluator',
        extraEvaluatorFields: [
          {name: 'trial_count', label: 'Trials per evaluation', type: 'int', default: 8, min: 1, max: 64},
          {name: 'width', label: 'Words to XOR (2-8)', type: 'int', default: 4, min: 2, max: 8},
          {name: 'max_steps_per_trial', label: 'VM steps per trial', type: 'int', default: 800, min: 1},
        ],
        evaluatorBuild: (v) => ({
          trial_count: v.trial_count, width: v.width,
          max_steps_per_trial: v.max_steps_per_trial,
        }),
        evaluatorParse: (p) => ({
          trial_count: p.trial_count, width: p.width || 4,
          max_steps_per_trial: p.max_steps_per_trial,
        }),
      }),
      numericEvaluatorPipe({
        type: 'BitReverseEvaluatorPipe',
        label: 'Bit-Reverse Evaluator',
        description:
          'Primitive gym: reverse the bit order of a single 32-bit input word (bit 0 ' +
          'becomes bit 31, etc.). Solvable in ~10-30 opcodes with shift+mask, or via the ' +
          'classic 5-stage Hacker\'s Delight swap pattern. Bit-Hamming scoring fits ' +
          'perfectly because the output is a full bitfield. A nice mid-difficulty target ' +
          'that pulls the population toward the shift/mask opcodes.',
        image: '/img/bit_reverse_evaluator_pipe.png',
        evaluatorType: 'BitReverseEvaluator',
        extraEvaluatorFields: [
          {name: 'trial_count', label: 'Trials per evaluation', type: 'int', default: 8, min: 1, max: 64},
          {name: 'max_steps_per_trial', label: 'VM steps per trial', type: 'int', default: 1500, min: 1},
        ],
        evaluatorBuild: (v) => ({
          trial_count: v.trial_count, max_steps_per_trial: v.max_steps_per_trial,
        }),
        evaluatorParse: (p) => ({
          trial_count: p.trial_count, max_steps_per_trial: p.max_steps_per_trial,
        }),
      }),
      numericEvaluatorPipe({
        type: 'MinimumEvaluatorPipe',
        label: 'Minimum Evaluator',
        description:
          'Primitive gym: return the smallest of N input words. Pushes the GA to ' +
          'discover the compare-and-swap pattern (CompareLessThan + branch + variable ' +
          'reassignment). Scoring uses log-scale numeric distance so "off by one" scores ' +
          'much closer to 1.0 than "off by a million" -- the gradient stays meaningful ' +
          'across the full uint32 range.',
        image: '/img/minimum_evaluator_pipe.png',
        evaluatorType: 'MinimumEvaluator',
        extraEvaluatorFields: [
          {name: 'trial_count', label: 'Trials per evaluation', type: 'int', default: 8, min: 1, max: 64},
          {name: 'width', label: 'Inputs to compare (2-8)', type: 'int', default: 4, min: 2, max: 8},
          {name: 'max_steps_per_trial', label: 'VM steps per trial', type: 'int', default: 1500, min: 1},
        ],
        evaluatorBuild: (v) => ({
          trial_count: v.trial_count, width: v.width,
          max_steps_per_trial: v.max_steps_per_trial,
        }),
        evaluatorParse: (p) => ({
          trial_count: p.trial_count, width: p.width || 4,
          max_steps_per_trial: p.max_steps_per_trial,
        }),
      }),
    ];
  })(),
  {
    type: 'NullSinkPipe',
    label: 'Null Sink',
    description: 'Consumes downstream output, throwing it away. Useful as the terminator of a pipeline.',
    image: '/img/null_sink_pipe.png',
    inputs: 1,
    outputs: 0,
    sections: [
      {
        title: 'Buffer',
        fields: [
          {name: 'max_candidates', label: 'Max candidates buffered', type: 'int', default: 100, min: 1},
        ],
      },
    ],
    buildParameters: (values) => ({max_candidates: values.max_candidates}),
    parseParameters: (params) => ({max_candidates: params.max_candidates}),
  },
  {
    type: 'ProgramStorageSinkPipe',
    label: 'Storage Sink',
    description:
      'Persists the top-K highest-scoring programs that flow through this pipe to a ' +
      'JSON ledger on disk. Use to checkpoint the best solutions so a later run can ' +
      'seed itself from them via a Storage Source.',
    image: '/img/program_storage_sink_pipe.png',
    inputs: 1,
    outputs: 0,
    sections: [
      {
        title: 'Destination',
        fields: [
          {name: 'max_candidates', label: 'Max input buffer', type: 'int', default: 50, min: 1},
          {name: 'path', label: 'Ledger file path', type: 'path',
           placeholder: 'Absolute path, e.g. /tmp/beast-best.json. Leave blank to log nothing.',
           default: ''},
          {name: 'top_k', label: 'Retain top K (0 = default 10)', type: 'int', default: 10, min: 0},
        ],
      },
    ],
    buildParameters: (values) => ({
      max_candidates: values.max_candidates,
      path: values.path || '',
      top_k: values.top_k,
    }),
    parseParameters: (params) => ({
      max_candidates: params.max_candidates,
      path: params.path || '',
      top_k: params.top_k != null ? params.top_k : 10,
    }),
  },
  {
    type: 'ProgramStorageSourcePipe',
    label: 'Storage Source',
    description:
      'Loads previously persisted programs from a JSON ledger and emits them as seed ' +
      'candidates. Feed into a Multiplexer alongside a Program Factory to start a new ' +
      'run with last run\'s survivors mixed in with fresh exploration.',
    image: '/img/program_storage_source_pipe.png',
    inputs: 0,
    outputs: 1,
    sections: [
      {
        title: 'Source',
        fields: [
          {name: 'max_candidates', label: 'Max output buffer', type: 'int', default: 50, min: 1},
          {name: 'path', label: 'Ledger file path', type: 'path',
           placeholder: 'Absolute path. Missing file = empty source (no-op).',
           default: ''},
          {name: 'loop', label: 'Loop entries (re-emit after exhausting the ledger)',
           type: 'bool', default: false},
        ],
      },
    ],
    buildParameters: (values) => ({
      max_candidates: values.max_candidates,
      path: values.path || '',
      loop: values.loop,
    }),
    parseParameters: (params) => ({
      max_candidates: params.max_candidates,
      path: params.path || '',
      loop: Boolean(params.loop),
    }),
  },
  {
    type: 'MultiplexerPipe',
    label: 'Multiplexer',
    description:
      'Merges candidates from several input slots into a single output stream. Use to ' +
      'combine fresh candidates from a factory with survivors looped back from a ' +
      'downstream stage, or to fan multiple producers into one consumer.',
    image: '/img/multiplexer_pipe.png',
    // Port count is dynamic: the input slot count is set per-instance via the form below.
    // Output is always 1 (that's the whole point of a mux).
    inputs: (params) => Math.max(1, Math.min(16, Number(params.input_slots || 2))),
    outputs: 1,
    sections: [
      {
        title: 'Slots',
        fields: [
          {name: 'max_candidates', label: 'Max candidates per slot', type: 'int', default: 50, min: 1},
          {name: 'input_slots', label: 'Input slots (1-16)', type: 'int', default: 2, min: 1, max: 16},
        ],
      },
    ],
    buildParameters: (values) => ({
      max_candidates: values.max_candidates,
      input_slots: values.input_slots,
    }),
    parseParameters: (params) => ({
      max_candidates: params.max_candidates,
      input_slots: params.input_slots,
    }),
  },
  {
    type: 'DemultiplexerPipe',
    label: 'Demultiplexer',
    description:
      'Splits a single input stream across several output slots. Round-robin spreads work ' +
      'evenly across parallel downstream branches; broadcast copies every candidate to ' +
      'every output (useful when evaluating the same population against several tasks); ' +
      'least-loaded routes to whichever output has the shortest queue, which keeps a ' +
      'slow branch from starving fast ones when downstream throughput is asymmetric.',
    image: '/img/demultiplexer_pipe.png',
    inputs: 1,
    outputs: (params) => Math.max(1, Math.min(16, Number(params.output_slots || 2))),
    sections: [
      {
        title: 'Slots',
        fields: [
          {name: 'max_candidates', label: 'Max candidates per slot', type: 'int', default: 50, min: 1},
          {name: 'output_slots', label: 'Output slots (1-16)', type: 'int', default: 2, min: 1, max: 16},
          {
            name: 'strategy',
            label: 'Distribution strategy',
            type: 'enum',
            options: [
              {value: 'round_robin', label: 'Round-robin (one candidate per branch, strict back-pressure)'},
              {value: 'broadcast', label: 'Broadcast (every branch sees every candidate)'},
              {value: 'least_loaded', label: 'Least-loaded (skip slow/full branches)'},
            ],
            default: 'round_robin',
          },
        ],
      },
    ],
    buildParameters: (values) => ({
      max_candidates: values.max_candidates,
      output_slots: values.output_slots,
      strategy: values.strategy,
    }),
    parseParameters: (params) => ({
      max_candidates: params.max_candidates,
      output_slots: params.output_slots,
      strategy: params.strategy,
    }),
  },
  {
    type: 'FilterPipe',
    label: 'Filter (by fitness)',
    description:
      'Routes each incoming candidate to one of two outputs based on its score: ' +
      'strictly below the threshold goes to output 0 (reject branch), equal-or-above ' +
      'goes to output 1 (pass branch). Useful for peeling survivors off into a ' +
      'storage sink or a promotion stage while feeding the rest back through a ' +
      'multiplexer for another round of mutation.',
    image: '/img/filter_pipe.png',
    inputs: 1,
    outputs: 2,
    sections: [
      {
        title: 'Filter',
        fields: [
          {name: 'max_candidates', label: 'Max candidates per slot', type: 'int', default: 50, min: 1},
          {
            name: 'threshold',
            label: 'Fitness threshold (score \u2265 passes; score < rejects)',
            type: 'float',
            default: 0.5,
            step: 0.01,
          },
        ],
      },
    ],
    buildParameters: (values) => ({
      max_candidates: values.max_candidates,
      threshold: values.threshold,
    }),
    parseParameters: (params) => ({
      max_candidates: params.max_candidates,
      threshold: params.threshold,
    }),
  },
  {
    type: 'FanPipe',
    label: 'Fan (throughput probe)',
    description:
      'Passthrough that measures candidate throughput over a sliding window. The icon ' +
      'spins faster as more candidates flow through, making contention points easy to ' +
      'spot at a glance. Pure observer -- forwards everything, drops nothing.',
    image: '/img/fan_pipe.png',
    inputs: 1,
    outputs: 1,
    sections: [
      {
        title: 'Measurement',
        fields: [
          {name: 'max_candidates', label: 'Max input buffer', type: 'int', default: 50, min: 1},
          {name: 'window_seconds', label: 'Throughput window (s, 0 = default 2s)',
           type: 'float', default: 2.0, min: 0, step: 0.1},
        ],
      },
    ],
    buildParameters: (values) => ({
      max_candidates: values.max_candidates,
      window_seconds: values.window_seconds,
    }),
    parseParameters: (params) => ({
      max_candidates: params.max_candidates,
      window_seconds: params.window_seconds != null ? params.window_seconds : 2.0,
    }),
  },
  {
    type: 'ResultsSummaryPipe',
    label: 'Results Summary',
    description:
      'Passthrough probe that reports score statistics (min/max/mean, best-ever, rolling ' +
      'window) for every candidate flowing through it. Put one right after an evaluator ' +
      'pipe to see how training is progressing, or between any two pipes to inspect ' +
      'intermediate scores.',
    image: '/img/results_summary_pipe.png',
    inputs: 1,
    outputs: 1,
    sections: [
      {
        title: 'Buffer',
        fields: [
          {name: 'max_candidates', label: 'Max candidates per cycle', type: 'int', default: 50, min: 1},
        ],
      },
      {
        title: 'Statistics',
        // Collapsed by default; the window-size default is good for most uses.
        collapsedByDefault: true,
        fields: [
          {name: 'window_size', label: 'Rolling window size (0 = default 256)', type: 'int', default: 0, min: 0},
        ],
      },
    ],
    buildParameters: (values) => ({
      max_candidates: values.max_candidates,
      window_size: values.window_size,
    }),
    parseParameters: (params) => ({
      max_candidates: params.max_candidates,
      window_size: params.window_size,
    }),
  },
  {
    type: 'ScoreGraphPipe',
    label: 'Score Graph',
    description:
      'Passthrough probe that records every candidate score along with the wall-clock ' +
      'time it was seen, then renders the resulting time-series as a sparkline on hover ' +
      'and a full-size line graph in the right-side summaries panel. Adjustable time ' +
      'window; resets on pipeline restart. Pure observer -- forwards everything, drops ' +
      'nothing.',
    image: '/img/score_graph_pipe.png',
    inputs: 1,
    outputs: 1,
    sections: [
      {
        title: 'Buffer',
        fields: [
          {name: 'max_candidates', label: 'Max candidates per cycle', type: 'int', default: 50, min: 1},
        ],
      },
      {
        title: 'Time-series window',
        // Most users will be happy with the defaults; only experts care to tune them.
        collapsedByDefault: true,
        fields: [
          {name: 'window_seconds', label: 'Graph window (s, 0 = default 60s)',
           type: 'float', default: 60.0, min: 0, step: 1.0},
          {name: 'max_samples', label: 'Max samples (0 = default 1024)',
           type: 'int', default: 1024, min: 0},
        ],
      },
    ],
    buildParameters: (values) => ({
      max_candidates: values.max_candidates,
      window_seconds: values.window_seconds,
      max_samples: values.max_samples,
    }),
    parseParameters: (params) => ({
      max_candidates: params.max_candidates,
      window_seconds: params.window_seconds != null ? params.window_seconds : 60.0,
      max_samples: params.max_samples != null ? params.max_samples : 1024,
    }),
  },
];

// Build a flat values object for the form when editing a pipe. Falls back to the
// schema-level defaults for any field the pipe's JSON doesn't carry, so we never feed
// `undefined` into a number input.
export function valuesFromExistingPipe(definition, pipe_json) {
  const parsed = definition.parseParameters
    ? definition.parseParameters((pipe_json && pipe_json.parameters) || {})
    : {};
  const values = defaultValuesFor(definition);
  for (const key of Object.keys(parsed)) {
    if (parsed[key] !== undefined && parsed[key] !== null) {
      values[key] = parsed[key];
    }
  }
  return values;
}

// Map back from a pipe's on-wire type (and its parameters, for the EvaluatorPipe ->
// MazeEvaluatorPipe disambiguation) to the matching definition.
export function findPipeDefinition(pipe_json) {
  if (!pipe_json || !pipe_json.type) return null;
  if (pipe_json.type === 'EvaluatorPipe') {
    const evals = pipe_json.parameters && pipe_json.parameters.evaluators;
    const firstType = evals && evals[0] && evals[0].type;
    const specialized = {
      MazeEvaluator: 'MazeEvaluatorPipe',
      AdderEvaluator: 'AdderEvaluatorPipe',
      MaximumEvaluator: 'MaximumEvaluatorPipe',
      IdentityEvaluator: 'IdentityEvaluatorPipe',
      BitwiseEvaluator: 'BitwiseEvaluatorPipe',
      RotateEvaluator: 'RotateEvaluatorPipe',
      Sha256SigmaEvaluator: 'Sha256SigmaEvaluatorPipe',
      Sha256ChEvaluator: 'Sha256ChEvaluatorPipe',
      PopcountEvaluator: 'PopcountEvaluatorPipe',
      ParityEvaluator: 'ParityEvaluatorPipe',
      BitReverseEvaluator: 'BitReverseEvaluatorPipe',
      MinimumEvaluator: 'MinimumEvaluatorPipe',
      Sha256MajEvaluator: 'Sha256MajEvaluatorPipe',
      Sha256RoundEvaluator: 'Sha256RoundEvaluatorPipe',
    }[firstType];
    if (specialized) {
      return PIPE_TYPE_DEFINITIONS.find(def => def.type === specialized);
    }
  }
  return PIPE_TYPE_DEFINITIONS.find(def => def.type === pipe_json.type);
}

// Resolve the input/output port counts for a pipe instance. Pipe types whose port count
// is fixed (e.g. NullSinkPipe is always 1 input / 0 outputs) just expose a number; types
// like MultiplexerPipe / DemultiplexerPipe whose ports depend on parameters expose a
// function that takes the pipe's parameters and returns the count. We default to 0 when
// nothing is configured so an unknown pipe still renders.
export function portCountsFor(definition, pipe_json) {
  if (!definition) return {inputs: 0, outputs: 0};
  const params = (pipe_json && pipe_json.parameters) || {};
  const resolve = (fieldOrFn) =>
    typeof fieldOrFn === 'function' ? Number(fieldOrFn(params) || 0) : Number(fieldOrFn || 0);
  return {
    inputs: resolve(definition.inputs),
    outputs: resolve(definition.outputs),
  };
}

// Default form values for a given definition; used both by AddPipeDialog when opening a
// fresh dialog and by EditPipeDialog as a fallback for missing keys.
export function defaultValuesFor(definition) {
  const values = {};
  for (const section of definition.sections || []) {
    for (const field of section.fields || []) {
      values[field.name] = field.default;
    }
  }
  return values;
}

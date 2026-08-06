// Program Collection page: surfaces every program ledger configured in the loaded
// pipelines and lets the user explore stored programs by score, view the disassembly,
// and generate a runnable, self-contained C source for each one.
//
// Layout: one expandable section per ledger -> one expandable row per program inside
// the ledger -> a tabbed view (overview / disassembly / C code) once expanded. The
// expanded body is lazy: disassembly is fetched eagerly because it's cheap and ships
// with the ledger, but the C code is generated on demand via a POST so we don't fan
// out CPU for programs the user never looks at.
//
// IMPORTANT: this file targets Material-UI v4 (see html_static/js/material-ui.production.min.js
// -- v4.12.4). v4 does NOT have `Stack` and does NOT support the `sx` prop. We use `Box`
// (with v4's `display`/`m`/`p`/`flexGrow` system props directly), and inline `style`
// objects for raw CSS on non-Box components. Default theme spacing is 8px per unit, so
// `m={2}` renders as 16px.

const {useEffect, useState, useCallback, useRef, createElement: e} = React;
const {
  Box,
  Button,
  CircularProgress,
  Collapse,
  Divider,
  IconButton,
  Paper,
  Tab,
  Table,
  TableBody,
  TableCell,
  TableContainer,
  TableHead,
  TableRow,
  Tabs,
  TextField,
  Tooltip,
  Typography,
} = MaterialUI;

// Tiny presentational widgets shared across the page. All trivially re-implementable;
// kept inline so this file doesn't pull in a sibling.
const SectionTitle = ({children, right}) =>
  e(Box, {display: 'flex', alignItems: 'center', mb: 1},
    e(Typography, {variant: 'h6', style: {flexGrow: 1}}, children),
    right || null,
  );

// Format a vector of byte values into a hex-grid string, 16 bytes per row, with the
// offset prefix that hex dumps traditionally carry. Used in the bytecode preview.
function formatHexDump(bytes) {
  const lines = [];
  for (let off = 0; off < bytes.length; off += 16) {
    const slice = bytes.slice(off, off + 16);
    const offHex = off.toString(16).padStart(6, '0');
    const hex = slice.map(b => b.toString(16).padStart(2, '0')).join(' ');
    // ASCII column on the right; printable -> char, otherwise '.'. Helps the user spot
    // embedded strings (e.g. a SetStringTableEntry payload) at a glance.
    const ascii = slice.map(b => (b >= 0x20 && b < 0x7F) ? String.fromCharCode(b) : '.').join('');
    lines.push(`${offHex}  ${hex.padEnd(48, ' ')}  ${ascii}`);
  }
  return lines.join('\n');
}

const MONO_STYLE = {fontFamily: 'monospace'};
const MONO_SMALL_STYLE = {fontFamily: 'monospace', fontSize: 12};
const MONO_TINY_STYLE = {fontFamily: 'monospace', fontSize: 11};
const BOLD_CELL_STYLE = {fontWeight: 'bold'};
const BOLD_MONO_CELL_STYLE = {fontWeight: 'bold', fontFamily: 'monospace'};

// One program inside one ledger. The body is mounted only when the row is expanded.
function ProgramRow({ledgerPath, program, index}) {
  const [expanded, setExpanded] = useState(false);
  const [tab, setTab] = useState(0);
  const [cCode, setCCode] = useState(null);
  const [cCodeLoading, setCCodeLoading] = useState(false);
  const [cCodeError, setCCodeError] = useState('');
  const [copyStatus, setCopyStatus] = useState('');

  // Lazy-generate the C source the first time the user opens the C-code tab. Cached on
  // the component until it unmounts, so flipping back and forth between tabs doesn't
  // re-hit the backend.
  const ensureCCode = useCallback(async () => {
    if (cCode !== null || cCodeLoading) return;
    setCCodeLoading(true);
    setCCodeError('');
    try {
      const response = await fetch('/api/v1/program-collection/c-code', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({
          data: program.data,
          source: `from ledger ${ledgerPath}\nrank #${index + 1}, score ${program.score.toFixed(6)}, ${program.size} bytes`,
        }),
      });
      const json = await response.json();
      if (!response.ok || json.status !== 'success') {
        setCCodeError(json.error || `HTTP ${response.status}`);
        return;
      }
      setCCode(json.c_code);
    } catch (err) {
      setCCodeError(err && err.message ? err.message : String(err));
    } finally {
      setCCodeLoading(false);
    }
  }, [cCode, cCodeLoading, ledgerPath, program, index]);

  const handleTabChange = (_, newValue) => {
    setTab(newValue);
    if (newValue === 2) {
      ensureCCode();
    }
  };

  const handleCopy = useCallback(async (text) => {
    try {
      await navigator.clipboard.writeText(text);
      setCopyStatus('Copied!');
      setTimeout(() => setCopyStatus(''), 1500);
    } catch {
      // Some browsers gate clipboard access on user activation; we deliberately don't
      // fall back to the old document.execCommand path because it's deprecated and
      // already incompatible with several modern browsers in some contexts.
      setCopyStatus('Copy failed -- select and Ctrl-C');
      setTimeout(() => setCopyStatus(''), 3000);
    }
  }, []);

  const headerCells = [
    e(TableCell, {key: 'rank', style: {width: 48, fontWeight: 'bold'}}, `#${index + 1}`),
    e(TableCell, {key: 'score'}, program.score.toFixed(6)),
    e(TableCell, {key: 'size'}, `${program.size} bytes`),
    e(TableCell, {key: 'disasm'},
      `${(program.disassembly || []).length} instr${program.disassembly_clean === false ? ' (truncated)' : ''}`),
    e(TableCell, {key: 'toggle', align: 'right'},
      e(IconButton, {size: 'small', onClick: () => setExpanded(!expanded), title: expanded ? 'Collapse' : 'Expand'},
        e('i', {className: 'material-icons'}, expanded ? 'expand_less' : 'expand_more'))),
  ];

  // Expanded tab body. Built as a switch to keep the JSX-ish tree below tractable.
  let tabBody = null;
  if (tab === 0) {
    tabBody = e(Box, null,
      e(Typography, {variant: 'subtitle2'}, 'Bytecode'),
      e(TextField, {
        multiline: true,
        value: formatHexDump(program.data || []),
        fullWidth: true,
        InputProps: {readOnly: true, style: MONO_SMALL_STYLE},
        rowsMax: 12,
        rows: Math.min(12, Math.max(4, Math.ceil((program.data || []).length / 16))),
      }),
      e(Box, {mt: 1},
        e(Button, {
          size: 'small',
          variant: 'outlined',
          onClick: () => handleCopy((program.data || []).map(b => b.toString(16).padStart(2, '0')).join(' ')),
        }, 'Copy hex bytes'),
        copyStatus && e(Typography, {variant: 'caption', style: {marginLeft: 8}}, copyStatus),
      ),
    );
  } else if (tab === 1) {
    tabBody = e(Box, null,
      e(TableContainer, {component: Paper, variant: 'outlined'},
        e(Table, {size: 'small'},
          e(TableHead, null,
            e(TableRow, null,
              e(TableCell, {style: BOLD_CELL_STYLE}, 'Offset'),
              e(TableCell, {style: BOLD_CELL_STYLE}, 'Instruction'),
              e(TableCell, {style: BOLD_MONO_CELL_STYLE}, 'Bytes'),
            )),
          e(TableBody, null,
            (program.disassembly || []).map((inst, i) =>
              e(TableRow, {key: 'inst' + i, hover: true},
                e(TableCell, {style: MONO_STYLE}, '0x' + inst.offset.toString(16).padStart(4, '0')),
                e(TableCell, {style: MONO_STYLE}, inst.text),
                e(TableCell, {style: {fontFamily: 'monospace', fontSize: 11, color: '#666'}},
                  inst.bytes_hex),
              )),
          ),
        )),
      program.disassembly_clean === false && e(Typography, {
        variant: 'caption', color: 'error',
        style: {marginTop: 8, display: 'block'},
      }, `Parser stopped early -- ${program.disassembly_trailing_garbage_bytes} trailing garbage byte(s).`),
    );
  } else if (tab === 2) {
    tabBody = e(Box, null,
      cCodeLoading && e(Box, {display: 'flex', alignItems: 'center', style: {gap: 8}},
        e(CircularProgress, {size: 16}),
        e(Typography, {variant: 'body2', style: {marginLeft: 8}}, 'Generating C source...')),
      cCodeError && e(Typography, {color: 'error', variant: 'body2'}, `Error: ${cCodeError}`),
      cCode && e(Box, null,
        e(Box, {display: 'flex', alignItems: 'center', mb: 1, style: {gap: 8}},
          e(Button, {
            size: 'small',
            variant: 'outlined',
            onClick: () => handleCopy(cCode),
          }, 'Copy C code'),
          copyStatus && e(Typography, {variant: 'caption', style: {marginLeft: 8}}, copyStatus),
          e(Typography, {
            variant: 'caption', color: 'textSecondary', style: {marginLeft: 8},
          }, `${cCode.length} chars -- compile with: cc program.c -o program`),
        ),
        e(TextField, {
          multiline: true,
          value: cCode,
          fullWidth: true,
          InputProps: {readOnly: true, style: MONO_TINY_STYLE},
          rowsMax: 30,
          rows: 10,
        }),
      ),
    );
  }

  return e(React.Fragment, null,
    e(TableRow, {hover: true, onClick: () => setExpanded(!expanded), style: {cursor: 'pointer'}},
      ...headerCells),
    e(TableRow, null,
      e(TableCell, {
        colSpan: 5,
        style: {padding: 0, borderBottom: expanded ? undefined : 'none'},
      },
        e(Collapse, {in: expanded, timeout: 'auto', unmountOnExit: true},
          e(Box, {p: 2, style: {backgroundColor: '#fafafa'}},
            e(Tabs, {
              value: tab, onChange: handleTabChange,
              indicatorColor: 'primary', textColor: 'primary',
              style: {borderBottom: '1px solid #e0e0e0', marginBottom: 16},
            },
              e(Tab, {label: 'Overview', key: 't0'}),
              e(Tab, {label: 'Disassembly', key: 't1'}),
              e(Tab, {label: 'C code', key: 't2'}),
            ),
            tabBody,
          ),
        ),
      ),
    ),
  );
}

// One ledger panel: header row showing the path + reference count, expandable body
// listing every program inside. The programs list itself is lazy-loaded the first
// time the user expands the section.
function LedgerPanel({ledger}) {
  const [expanded, setExpanded] = useState(false);
  const [programs, setPrograms] = useState(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState('');
  const loadedFor = useRef(null); // remember which path we loaded for so re-render keeps cache

  const loadPrograms = useCallback(async () => {
    if (loading) return;
    setLoading(true);
    setError('');
    try {
      const response = await fetch(`/api/v1/program-collection/ledger?path=${encodeURIComponent(ledger.path)}`);
      const json = await response.json();
      if (!response.ok || json.status !== 'success') {
        setError(json.error || `HTTP ${response.status}`);
        setPrograms([]);
        return;
      }
      setPrograms(json.programs || []);
      loadedFor.current = ledger.path;
    } catch (err) {
      setError(err && err.message ? err.message : String(err));
    } finally {
      setLoading(false);
    }
  }, [ledger.path, loading]);

  const handleToggle = () => {
    const next = !expanded;
    setExpanded(next);
    if (next && (programs === null || loadedFor.current !== ledger.path)) {
      loadPrograms();
    }
  };

  return e(Paper, {variant: 'outlined', style: {marginBottom: 16, padding: 0}},
    e(Box, {
      p: 2, display: 'flex', alignItems: 'center',
      style: {
        cursor: 'pointer',
        borderBottom: expanded ? '1px solid #ddd' : 'none',
      },
      onClick: handleToggle,
    },
      e(IconButton, {size: 'small'},
        e('i', {className: 'material-icons'}, expanded ? 'expand_less' : 'expand_more')),
      e(Box, {style: {flexGrow: 1, marginLeft: 8}},
        e(Typography, {variant: 'subtitle1', style: MONO_STYLE}, ledger.path),
        e(Typography, {variant: 'caption', color: 'textSecondary'},
          `${ledger.program_count} program${ledger.program_count === 1 ? '' : 's'} \u2022 ` +
          `${ledger.exists ? `${ledger.size_bytes} bytes on disk` : 'file not yet created'} \u2022 ` +
          `${(ledger.references || []).length} reference${(ledger.references || []).length === 1 ? '' : 's'}`),
      ),
      e(Tooltip, {title: 'Refresh', placement: 'left'},
        e(IconButton, {
          size: 'small',
          onClick: (ev) => { ev.stopPropagation(); loadPrograms(); },
        }, e('i', {className: 'material-icons'}, 'refresh'))),
    ),
    e(Collapse, {in: expanded, timeout: 'auto', unmountOnExit: true},
      e(Box, {p: 2},
        // Reference list -- which pipelines / pipes use this ledger, sink or source
        (ledger.references || []).length > 0 && e(Box, {mb: 2},
          e(Typography, {variant: 'caption', color: 'textSecondary'}, 'Referenced by:'),
          e(Box, {
            display: 'flex',
            style: {flexWrap: 'wrap', marginTop: 4, gap: 4},
          },
            (ledger.references || []).map((ref, i) =>
              e(Box, {
                key: 'ref' + i,
                style: {
                  paddingLeft: 8, paddingRight: 8, paddingTop: 2, paddingBottom: 2,
                  fontSize: 11,
                  backgroundColor: ref.role === 'sink' ? '#e3f2fd' : '#f3e5f5',
                  border: '1px solid #ccc', borderRadius: 4, marginRight: 4, marginBottom: 4,
                },
              }, `${ref.pipeline_name} / ${ref.pipe_name} (${ref.role})`),
            ),
          ),
        ),
        loading && e(Box, {display: 'flex', alignItems: 'center'},
          e(CircularProgress, {size: 16}),
          e(Typography, {variant: 'body2', style: {marginLeft: 8}}, 'Loading programs...')),
        error && e(Typography, {color: 'error', variant: 'body2'}, `Error: ${error}`),
        !loading && !error && programs !== null && programs.length === 0 &&
          e(Typography, {variant: 'body2', color: 'textSecondary'},
            'No programs in this ledger yet. They appear here as the sink writes them.'),
        !loading && !error && programs !== null && programs.length > 0 &&
          e(TableContainer, {component: Paper, variant: 'outlined'},
            e(Table, {size: 'small'},
              e(TableHead, null,
                e(TableRow, null,
                  e(TableCell, {style: BOLD_CELL_STYLE}, 'Rank'),
                  e(TableCell, {style: BOLD_CELL_STYLE}, 'Score'),
                  e(TableCell, {style: BOLD_CELL_STYLE}, 'Size'),
                  e(TableCell, {style: BOLD_CELL_STYLE}, 'Disassembly'),
                  e(TableCell, null, ''),
                )),
              e(TableBody, null,
                programs.map((p, i) =>
                  e(ProgramRow, {key: 'p' + i, ledgerPath: ledger.path, program: p, index: i})),
              ),
            ),
          ),
      ),
    ),
  );
}

export function ProgramCollection() {
  const [ledgers, setLedgers] = useState(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState('');

  const refresh = useCallback(async () => {
    setLoading(true);
    setError('');
    try {
      const response = await fetch('/api/v1/program-collection');
      const json = await response.json();
      if (!response.ok || json.status !== 'success') {
        setError(json.error || `HTTP ${response.status}`);
        setLedgers([]);
        return;
      }
      setLedgers(json.ledgers || []);
    } catch (err) {
      setError(err && err.message ? err.message : String(err));
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => { refresh(); }, [refresh]);

  return e(Box, {style: {paddingBottom: 32}},
    e(SectionTitle, {
      right: e(Button, {
        size: 'small', onClick: refresh,
        startIcon: e('i', {className: 'material-icons'}, 'refresh'),
      }, 'Refresh'),
    }, 'Program Collection'),
    e(Typography, {variant: 'body2', color: 'textSecondary', style: {marginBottom: 16}},
      'All program ledgers referenced by storage sinks and sources across your pipelines. ' +
      'Click a ledger to see the programs it has captured. Click any program to inspect its ' +
      'bytecode, disassembly, or to copy a self-contained C source ready for compilation.'),
    loading && !ledgers && e(Box, {display: 'flex', alignItems: 'center'},
      e(CircularProgress, {size: 18}),
      e(Typography, {style: {marginLeft: 8}}, 'Loading ledgers...')),
    error && e(Typography, {color: 'error', variant: 'body2'}, `Error: ${error}`),
    !loading && !error && ledgers && ledgers.length === 0 &&
      e(Paper, {variant: 'outlined', style: {padding: 24, textAlign: 'center'}},
        e(Typography, {color: 'textSecondary'},
          'No ledgers configured yet. Add a ProgramStorageSink or ProgramStorageSource pipe ' +
          'to one of your pipelines (with a non-empty path) to see it here.')),
    ledgers && ledgers.length > 0 && e(Divider, {style: {marginBottom: 16}}),
    ledgers && ledgers.map((l) => e(LedgerPanel, {key: l.path, ledger: l})),
  );
}

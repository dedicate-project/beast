const {useState, createElement : e, useEffect, useRef, useLayoutEffect} = React;
const {
  Typography,
  Box,
  Button,
  TextField,
  Toolbar,
  IconButton,
  makeStyles,
  Dialog,
  DialogTitle,
  DialogContent,
  DialogActions
} = MaterialUI;
const {createPortal} = ReactDOM;
const {Stage, Layer, Rect} = Konva;

import {ContextMenu} from './ContextMenu.js';
import {AddPipeDialog} from './AddPipeDialog.js';
import {ConfirmationDialog} from './ConfirmationDialog.js';
import {PIPE_TYPE_DEFINITIONS, findPipeDefinition} from './PipeTypes.js';

const useStyles = makeStyles((theme) => ({
                               toolbar : {
                                 display : "flex",
                                 justifyContent : "space-between",
                                 marginBottom : theme.spacing(2),
                                 alignItems : "center",
                               },
                               pipelineName : {
                                 fontWeight : "bold",
                                 fontSize : "1.5rem",
                                 marginLeft : theme.spacing(1),
                               },
                             }));

// Previous `onResize`/`useResize` pair referenced an undefined `callback` symbol and
// ignored its actual argument. Replaced with a self-contained hook that keeps the latest
// callback in a ref so the resize listener doesn't have to re-register on every render.
const useResize = (callback) => {
  const cbRef = useRef(callback);
  useEffect(() => { cbRef.current = callback; }, [ callback ]);
  useEffect(() => {
    const handler = () => { if (cbRef.current) cbRef.current(); };
    window.addEventListener("resize", handler);
    return () => window.removeEventListener("resize", handler);
  }, []);
};

function drawGrid(layer, width, height, gridSize, gridSizeMultiplier) {
  const extendedWidth = width * gridSizeMultiplier;
  const extendedHeight = height * gridSizeMultiplier;

  for (let i = 0; i < extendedWidth; i += gridSize) {
    const verticalLine = new Konva.Line({
      points : [ i, 0, i, extendedHeight ],
      stroke : "lightgrey",
      strokeWidth : 1,
    });
    layer.add(verticalLine);
  }

  for (let i = 0; i < extendedHeight; i += gridSize) {
    const horizontalLine = new Konva.Line({
      points : [ 0, i, extendedWidth, i ],
      stroke : "lightgrey",
      strokeWidth : 1,
    });
    layer.add(horizontalLine);
  }
}

function drawBorder(layer, width, height) {
  const border = new Konva.Rect({
    x : 0,
    y : 0,
    width : width,
    height : height,
    stroke : "black",
    strokeWidth : 1,
  });

  layer.add(border);
}

export function PipelineCanvas({pipeline, onBackButtonClick}) {
  const classes = useStyles();
  const [pipelineState, setPipelineState] = useState(pipeline.state);
  const pipelineStateRef = useRef("");
  const [dimensions, setDimensions] =
      useState({width : window.innerWidth - 320, height : window.innerHeight - 200});
  const [stageInstance, setStageInstance] = useState(null);
  const [dragging, setDragging] = useState(false);
  const [dragMovePoint, setDragMovePoint] = useState({x : 0, y : 0});

  const [showContextMenu, setShowContextMenu] = useState(false);
  const [contextMenuPosition, setContextMenuPosition] = useState({x : 0, y : 0});

  const [renameDialogOpen, setRenameDialogOpen] = useState(false);
  const [newPipelineName, setNewPipelineName] = useState(pipeline.name);
  const [showEditButton, setShowEditButton] = useState(false);

  const [oldModel, setOldModel] = useState({});
  const [model, setModel] = useState({});
  const [metadata, setMetadata] = useState({});

  const [pipelineName, setPipelineName] = useState(pipeline.name);

  const stageRef = useRef(null);
  const gridLayerRef = useRef(null);
  const borderLayerRef = useRef(null);
  const elementLayerRef = useRef(null);
  const connectionsLayerRef = useRef(null);
  const draggingRef = useRef(false);
  const lastDragPointRef = useRef({x : 0, y : 0});

  const [pipes, setPipes] = useState({});
  const [currentlyDraggedPipe, setCurrentlyDraggedPipe] = useState("");

  // Authoring state. `addPipeOpen` controls the new-pipe modal; `addPipeAnchor` carries the
  // canvas coordinate the user right-clicked, so the new pipe lands roughly where they
  // asked for it. `pendingDelete` carries the name of a pipe queued for delete (we route
  // through ConfirmationDialog so accidental clicks don't blow away a 20-knob configured
  // EvaluatorPipe). `pendingConnectionSource` tracks the source port of an in-progress
  // click-source-then-click-destination connection authoring gesture; `null` means "no
  // connection in flight, port clicks should start a new gesture instead of completing
  // one". `pendingDeleteConnection` is the analogous queue for connection deletes.
  const [addPipeOpen, setAddPipeOpen] = useState(false);
  const [addPipeAnchor, setAddPipeAnchor] = useState(null);
  const [pendingDelete, setPendingDelete] = useState(null);
  const [pendingDeleteConnection, setPendingDeleteConnection] = useState(null);
  const [pendingConnectionSource, setPendingConnectionSource] = useState(null);
  const pendingConnectionSourceRef = useRef(null);
  const [apiError, setApiError] = useState("");
  // When non-null, the AddPipeDialog renders in edit mode against this pipe; shape
  // `{name, pipe_json}` so the dialog can prefill the form via valuesFromExistingPipe.
  const [editingPipe, setEditingPipe] = useState(null);

  React.useEffect(() => {
    const handleContextMenu = (e) => { e.preventDefault(); };

    window.addEventListener("contextmenu", handleContextMenu);

    return () => { window.removeEventListener("contextmenu", handleContextMenu); };
  }, []);

  const [metrics, setMetrics] = useState({});
  // Position of the floating pipe-info tooltip, in viewport coordinates.
  const [tooltipPos, setTooltipPos] = useState({x : 0, y : 0});

  const rightClickMenuItemsRef = useRef([])

  // Renamed from the previous local `Dialog` definition, which silently shadowed
  // MaterialUI.Dialog and broke the pipeline-rename modal at the bottom of the render tree.
  const PipeInfoTooltip = (props) => {
    if (!props.hoveredPipe) {
      return null;
    }
    const pipesMetrics = metrics && Array.isArray(metrics["pipes"]) ? metrics["pipes"] : [];
    const pipe = pipesMetrics.find(p => p.name === props.hoveredPipe);
    const modelPipes = model && model["pipes"] ? model["pipes"] : {};
    const modelPipe = modelPipes[props.hoveredPipe];

    let content = [ React.createElement('strong', {key : 'name'}, props.hoveredPipe) ];
    if (modelPipe && modelPipe["type"]) {
      content.push(React.createElement('div', {key : 'type'}, 'Type: ' + modelPipe["type"]));
    }

    if (pipe) {
      content.push(React.createElement(
          'div', {key : 'exec'},
          `Executions: ${parseFloat(pipe.execution_count).toFixed(2)} / s`));

      const inputs = (pipe.inputs || []).map((input, index) => {
        return e('div', {key : 'in' + index},
                 `* Input ${index}: ${parseFloat(input).toFixed(2)} / s`);
      });
      const outputs = (pipe.outputs || []).map((output, index) => {
        return e('div', {key : 'out' + index},
                 `* Output ${index}: ${parseFloat(output).toFixed(2)} / s`);
      });
      if (inputs.length > 0) {
        content.push(React.createElement('div', {key : 'inh'}, 'Inputs:'), ...inputs);
      }
      if (outputs.length > 0) {
        content.push(React.createElement('div', {key : 'outh'}, 'Outputs:'), ...outputs);
      }

      // ResultsSummaryPipe attaches a `summary` block to its metrics entry. Surface the
      // headline stats inline in the tooltip so a quick mouse-over tells the user how
      // training is going without opening the dedicated panel.
      if (pipe.summary) {
        const s = pipe.summary;
        const fmt = (v) => Number.isFinite(v) ? Number(v).toFixed(3) : '--';
        content.push(
            React.createElement('div', {key : 'sumh', style : {marginTop : '0.4rem'}},
                                'Score summary:'),
            React.createElement('div', {key : 'sumn'},
                                `* Seen total / window: ${s.count_total} / ${s.count_window}`),
            React.createElement('div', {key : 'sumr'},
                                `* min / mean / max: ${fmt(s.min_score)} / ${fmt(s.mean_score)} / ${fmt(s.max_score)}`),
            React.createElement('div', {key : 'suml'},
                                `* Last: ${fmt(s.last_score)}`),
            React.createElement('div', {key : 'sumb'},
                                `* Best ever: ${fmt(s.best_ever_score)} (${s.best_ever_size} bytes)`));
      }
    }
    return e('div', {
      className : 'pipe-dialog',
      style : {
        position : 'fixed',
        left : props.x + 'px',
        top : props.y + 'px',
        backgroundColor : 'white',
        border : '1px solid black',
        borderRadius : '4px',
        boxShadow : '0px 4px 6px rgba(0, 0, 0, 0.1)',
        fontFamily : 'sans-serif',
        minWidth : '200px',
        padding : '1rem',
        zIndex : 9999,
        pointerEvents : 'none'
      }
    },
             content);
  };

  const [hoveredPipe, setHoveredPipe] = useState("");
  const hoveredPipeRef = useRef("");

  // Single entry point for all mutating REST calls. Centralised so the API-error toast and
  // the post-mutation pipeline re-fetch live in one place; every action handler below uses
  // it and treats the boolean return as "was the server happy?".
  const postUpdate = async (body) => {
    try {
      const response = await fetch(`/api/v1/pipelines/${pipeline.id}/update`, {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(body),
      });
      const data = await response.json();
      if (!response.ok || data.status !== 'success') {
        const reason = data.error || `HTTP ${response.status}`;
        setApiError(`${body.action} failed: ${reason}`);
        return false;
      }
      // Kick a refresh so the change shows up immediately instead of after the next 1s
      // poll cycle. We deliberately ignore the result because the polling loop will pick
      // up any state we miss; this is purely a latency optimisation.
      refreshPipelineState();
      setApiError('');
      return true;
    } catch (e) {
      setApiError(`${body.action} failed: ${e && e.message ? e.message : e}`);
      return false;
    }
  };

  // Stable handle so polling and on-demand fetches share the same code path. The actual
  // implementation lives inside the polling useEffect; we expose it through this ref so
  // postUpdate (declared above the polling effect for ordering reasons) can call it.
  const refreshPipelineStateRef = useRef(() => {});
  const refreshPipelineState = () => refreshPipelineStateRef.current();

  function getRightClickMenuItems(pipe) {
    const running = pipelineStateRef.current === "running";
    if (pipe === "") {
      return [
        {
          text : "Add new Pipe",
          icon : "add",
          disabled : running,
          action : () => {
            setEditingPipe(null);
            setAddPipeAnchor(addPipeAnchorRef.current);
            setAddPipeOpen(true);
          },
        },
      ];
    }
    return [
      {
        text : "Edit " + pipe,
        icon : "edit",
        disabled : running,
        action : () => {
          // Pull the latest model snapshot rather than closing over a stale prop, since
          // this lambda runs through `rightClickMenuItemsRef` which is set once per click.
          const modelPipes = (model && model.pipes) || {};
          if (modelPipes[pipe]) {
            setEditingPipe({name: pipe, pipe_json: modelPipes[pipe]});
            setAddPipeOpen(true);
          }
        },
      },
      {
        text : "Delete " + pipe,
        icon : "delete",
        disabled : running,
        action : () => setPendingDelete(pipe),
      },
    ];
  }

  // Stash the canvas-space coordinate of the latest right-click so the Add Pipe dialog can
  // use it as the starting position for the new pipe. Updated synchronously inside the
  // stage mousedown handler.
  const addPipeAnchorRef = useRef({x: 50, y: 50});

  // Cancel an in-progress connection authoring gesture (called on Escape, click on empty
  // canvas, or when the source pipe gets deleted out from under us).
  const cancelPendingConnection = () => {
    pendingConnectionSourceRef.current = null;
    setPendingConnectionSource(null);
  };

  // Previously this rendered a separate React tree into a manually-created `#dialog-root`
  // div via the legacy `ReactDOM.render` API. That API is deprecated under React 18 and the
  // dialog root div was being recreated on every render (the `document.createElement` call
  // used to sit at the top level of the component body and ran for each pass). We now use
  // `createPortal` from inside the regular tree, see the JSX at the bottom of the render.

  useEffect(() => {
    const stage = new Konva.Stage({
      container : stageRef.current,
      width : dimensions.width,
      height : dimensions.height,
    });
    setStageInstance(stage);

    gridLayerRef.current = new Konva.Layer({
      x : -dimensions.width,  // Negative offset by half of the width
      y : -dimensions.height, // Negative offset by half of the height
    });
    drawGrid(gridLayerRef.current, dimensions.width, dimensions.height, 20,
             3); // Grid size = 20, gridSizeMultiplier = 3
    stage.add(gridLayerRef.current);

    connectionsLayerRef.current = new Konva.Layer();
    stage.add(connectionsLayerRef.current);

    elementLayerRef.current = new Konva.Layer();
    stage.add(elementLayerRef.current);

    borderLayerRef.current = new Konva.Layer({listening : false});
    drawBorder(borderLayerRef.current, dimensions.width, dimensions.height);
    stage.add(borderLayerRef.current);

    stage.on("mousedown", (e) => {
      // Check if the middle mouse button is pressed
      if (e.evt.button === 1) {
        setDragging(true);
        draggingRef.current = true;
        lastDragPointRef.current = stage.getPointerPosition();
      } else if (e.evt.button === 2) {
        e.evt.preventDefault();           // This prevents the native context menu
        e.evt.stopPropagation();
        e.evt.stopImmediatePropagation();
        e.evt.cancelBubble = true;
        const position = stage.getPointerPosition();

        // Translate the pointer position from stage-space into element-layer-space (the
        // layer is panned by the middle-mouse drag handler, so we subtract its offset).
        // This is the coordinate we hand off to AddPipeDialog so the new pipe lands where
        // the user actually right-clicked.
        const layer = elementLayerRef.current;
        if (layer) {
          addPipeAnchorRef.current = {
            x: position.x - layer.x(),
            y: position.y - layer.y(),
          };
        }

        const container = stage.container();
        const rect = container.getBoundingClientRect();
        const adjustedPosition = {
          x : position.x + rect.left + 5,
          y : position.y - 2 * rect.top + 5
        };

        rightClickMenuItemsRef.current = getRightClickMenuItems(hoveredPipeRef.current);
        setContextMenuPosition(adjustedPosition);
        setShowContextMenu(true);
      } else if (e.evt.button === 0) {
        // Left-click on empty canvas cancels any in-flight connection authoring gesture.
        // We compare the event target to the stage itself rather than to a layer so port
        // clicks (which bubble up from a Konva.Rect inside a Group) still complete
        // connections normally.
        if (e.target === stage && pendingConnectionSourceRef.current) {
          cancelPendingConnection();
        }
      }
    });

    stage.on("mousemove", (e) => {
      if (draggingRef.current) {
        const pointerPosition = stage.getPointerPosition();
        const dx = pointerPosition.x - lastDragPointRef.current.x;
        const dy = pointerPosition.y - lastDragPointRef.current.y;

        elementLayerRef.current.x(elementLayerRef.current.x() + dx);
        elementLayerRef.current.y(elementLayerRef.current.y() + dy);
        gridLayerRef.current.x(gridLayerRef.current.x() + dx);
        gridLayerRef.current.y(gridLayerRef.current.y() + dy);
        connectionsLayerRef.current.x(connectionsLayerRef.current.x() + dx);
        connectionsLayerRef.current.y(connectionsLayerRef.current.y() + dy);

        elementLayerRef.current.batchDraw();
        gridLayerRef.current.batchDraw();

        lastDragPointRef.current = pointerPosition;
      }
    });

    stage.on("mouseup", () => {
      draggingRef.current = false;
      setCurrentlyDraggedPipe("");
    });

    stage.on("mouseout", () => {
      draggingRef.current = false;
      setCurrentlyDraggedPipe("");
    });

    // Clean up the Konva stage when the canvas unmounts (e.g. user navigates back to the
    // pipeline list). Without this the entire stage tree, image objects, and event handlers
    // stay attached to the detached container div forever.
    return () => {
      stage.destroy();
      gridLayerRef.current = null;
      borderLayerRef.current = null;
      elementLayerRef.current = null;
      connectionsLayerRef.current = null;
    };
  }, []);

  function getPipeNameForKonvaImage(konvaImage) {
    for (const key in pipes) {
      if (pipes[key] == konvaImage) {
        return key;
      }
    }
    return null;
  }

  function handlePipeDragEnded(konvaImage) {
    const pipe_name = getPipeNameForKonvaImage(konvaImage);
    if (pipe_name == null) {
      console.log('Pipe not found for image');
      return;
    }
    setCurrentlyDraggedPipe("");
    fetch(`/api/v1/pipelines/${pipeline.id}/update`, {
      method : "POST",
      headers : {
        "Content-Type" : "application/json",
      },
      body : JSON.stringify(
          {action : "move_pipe", name : pipe_name, x : konvaImage.getX(), y : konvaImage.getY()}),
    });
  }

  useEffect(() => {
    // Load an image and create a draggable object
    const createDraggableImage = async (src, x, y, inports, outports, name, type) => {
      // create the group
      var group = new Konva.Group({x : x, y : y, draggable : true});

      group.ports = {inputs : [], outputs : []};

      // create the Rect object
      var item = new Konva.Rect({
        x : 0,
        y : 0,
        width : 50,
        height : 50,
        fill : '#ccc',    // light grey fill color
        stroke : '#333',  // dark grey border color
        strokeWidth : 1,  // border width
        cornerRadius : 5, // rounded corner radius
      });

      // add the Rect object to the group
      group.add(item);

      // add ports
      const addPort = (side, slot, max_slots) => {
        const portHeight = 10;
        const portWidth = 3;
        const portDistance = 5;
        const x = (side == "input" ? -portWidth : 50);
        const y =
            (slot * (portHeight + portDistance) - portDistance + 25) -
            (max_slots > 1 ? (max_slots * portHeight - (max_slots - 1) * portDistance) / 2 : 0);

        var portRect = new Konva.Rect({
          x : x,
          y : y,
          width : portWidth,
          height : portHeight,
          fill : '#ccc',
          stroke : '#333',
          strokeWidth : 1,
          cornerRadius : 0,
          // Slightly larger invisible hit region than the visible 3x10 rectangle so the
          // user doesn't have to click pixel-perfectly to start a connection. CRITICAL:
          // the expansion grows away from the pipe body, never into it -- if it grew
          // inward we'd steal mousedowns from the body and break native Konva dragging
          // (which is the entire reason pipes can be repositioned on the canvas).
          hitFunc: function(context) {
            const padOutward = 6;
            const padVertical = 4;
            if (side === 'input') {
              // Body sits at x>=0; grow leftward only.
              context.beginPath();
              context.rect(-padOutward, -padVertical, portWidth + padOutward,
                           portHeight + 2 * padVertical);
              context.closePath();
            } else {
              // Output port sits at x=50 (body's right edge); grow rightward only.
              context.beginPath();
              context.rect(0, -padVertical, portWidth + padOutward,
                           portHeight + 2 * padVertical);
              context.closePath();
            }
            context.fillStrokeShape(this);
          },
        });

        portRect.portInfo = {pipeName: name, side, slot};

        portRect.on('mouseover', (event) => {
          portRect.fill('#ff8c00');
          portRect.getLayer().batchDraw();
          event.cancelBubble = true;
        });
        portRect.on('mouseout', () => {
          portRect.fill('#ccc');
          portRect.getLayer().batchDraw();
        });
        // Stop the parent group's drag from starting when the user is mid-connection,
        // i.e. either about to start one (clicking a fresh output port) or about to
        // finish one (clicking a destination input). At rest -- no pending connection --
        // we let the mousedown propagate so a click on the port doesn't disable nearby
        // canvas dragging when the user mis-aims.
        portRect.on('mousedown', (event) => {
          if (event.evt.button !== 0) return;
          if (side === 'output' || pendingConnectionSourceRef.current) {
            event.cancelBubble = true;
            event.evt.stopPropagation();
            // Disengage any drag Konva had already started in the same tick -- without
            // this, holding the mouse after a port click would still initiate a drag.
            const groupNode = portRect.getParent();
            if (groupNode && groupNode.isDragging && groupNode.isDragging()) {
              groupNode.stopDrag();
            }
          }
        });
        portRect.on('click', (event) => {
          if (event.evt.button !== 0) return;
          event.cancelBubble = true;
          const pending = pendingConnectionSourceRef.current;
          if (!pending) {
            // Begin a new connection. Only outputs can start one; clicking an input first
            // would have ambiguous semantics (you can't have a connection that flows
            // "backwards"), and starting from inputs makes the wrong end orange.
            if (side !== 'output') return;
            pendingConnectionSourceRef.current = {pipeName: name, slot};
            setPendingConnectionSource({pipeName: name, slot});
          } else {
            // Complete a connection. Mirror restriction: only an input can be the
            // destination. Reject inputs on the same pipe to keep the model acyclic by
            // default (loops are technically supported in the C++ core but a UI-driven
            // self-loop is almost always a mis-click).
            if (side !== 'input') return;
            if (pending.pipeName === name) {
              setApiError('Cannot connect a pipe to itself.');
              return;
            }
            postUpdate({
              action: 'add_connection',
              source_pipe: pending.pipeName,
              source_slot: pending.slot,
              destination_pipe: name,
              destination_slot: slot,
              // Use the same buffer size as the pipelines example. Future PR: expose it
              // via the connection right-click menu.
              buffer_size: 50,
            });
            cancelPendingConnection();
          }
        });

        group.add(portRect);

        return portRect;
      };

      for (var inport = 0; inport < inports; inport++) {
        var port = addPort("input", inport, inports);
        group.ports.inputs.push(port);
      }
      for (var outport = 0; outport < outports; outport++) {
        var port = addPort("output", outport, outports);
        group.ports.outputs.push(port);
      }

      // load the image
      var imageObj = new Image();
      imageObj.crossOrigin = "anonymous";
      imageObj.onload = function() {
        var img = new Konva.Image({
          x : (item.width() - 50) / 2 + 5,
          y : (item.height() - 50) / 2 + 5,
          image : imageObj,
          width : 40,
          height : 40,
        });

        // add the Konva.Image object to the group
        group.add(img);

        // redraw the layer to show the updated rectangle and image
        elementLayerRef.current.draw();
      };
      imageObj.src = src;

      // add the group to the layer
      elementLayerRef.current.add(group);
      elementLayerRef.current.batchDraw();

      // add event listeners to the object
      group.on('mouseover', () => {
        hoveredPipeRef.current = name;
        setHoveredPipe(name);
      });

      group.on('mouseout', () => {
        hoveredPipeRef.current = "";
        setHoveredPipe("");
      });

      group.on('mousemove dragmove', (event) => {
        // The tooltip is rendered through a React portal (see PipeInfoTooltip + createPortal
        // at the bottom of the render). We only update the desired position here; React
        // takes care of styling the portal child accordingly. Final clamping against the
        // viewport happens in the tooltip's render so we don't read its offsetWidth here
        // (which would race the React commit).
        setTooltipPos({x : event.evt.clientX + 12, y : event.evt.clientY + 12});
      });

      group.on("mousedown", (e) => {
        if (e.evt.button !== 0) {
          e.target.stopDrag();
        }
      });

      group.on("dragstart", (e) => {
        setCurrentlyDraggedPipe(getPipeNameForKonvaImage(e.target));
        const pointerPosition = stageInstance.getPointerPosition();
        const pixel = e.target.getLayer()
                          .getContext()
                          .getImageData(pointerPosition.x, pointerPosition.y, 1, 1)
                          .data;
        if (pixel[3] === 0) {
          e.target.stopDrag();
        }
      });
      group.on("dragend", (e) => { handlePipeDragEnded(group); });
      group.on("dragmove", (e) => { setDragMovePoint({x : e.target.x(), y : e.target.y()}); });

      return group;
    };

    // Compare oldModel to model and only update the pipes that changed!
    // Defend against the backend reporting `"pipes": null` or `model: null` for empty
    // pipelines; both `null` and `undefined` would otherwise blow up `for ... in`.
    const safeModel = (model && typeof model === 'object') ? model : {};
    const safeOldModel = (oldModel && typeof oldModel === 'object') ? oldModel : {};
    if (!safeModel["pipes"]) safeModel["pipes"] = {};
    if (!safeOldModel["pipes"]) safeOldModel["pipes"] = {};
    var added_pipes = {};
    var updated_pipes = {};
    var removed_pipes = {};
    for (let key in safeModel["pipes"]) {
      // Check if in old model
      if (key in safeOldModel["pipes"]) {
        if (safeModel["pipes"][key] != safeOldModel["pipes"][key]) {
          updated_pipes[key] = safeModel["pipes"][key];
        }
      } else {
        added_pipes[key] = safeModel["pipes"][key];
      }
    }
    for (let key in safeOldModel["pipes"]) {
      if (!(key in safeModel["pipes"])) {
        removed_pipes[key] = safeOldModel["pipes"][key];
      }
    }
    setOldModel(model);

    for (let key in added_pipes) {
      // Look up the pipe's appearance via the central PipeTypes definition so adding new
      // pipe categories only requires one new entry. Unknown types still render with the
      // legacy plain icon and no ports so they don't crash the canvas.
      const definition = findPipeDefinition(added_pipes[key]);
      const inports = definition ? definition.inputs : 0;
      const outports = definition ? definition.outputs : 0;
      const image_file = (definition && definition.image) ||
                         "/img/pipe_plain_oneinputoneoutput.png";
      var pos_x = 50;
      var pos_y = 50;
      const safeMetaInner = (metadata && typeof metadata === 'object') ? metadata : {};
      if (safeMetaInner["pipes"] && safeMetaInner["pipes"][key] &&
          safeMetaInner["pipes"][key]["position"]) {
        pos_x = safeMetaInner["pipes"][key]["position"]["x"];
        pos_y = safeMetaInner["pipes"][key]["position"]["y"];
      }

      createDraggableImage(image_file, pos_x, pos_y, inports, outports, key,
                           added_pipes[key] && added_pipes[key]["type"])
          .then((konvaImage) => { pipes[key] = konvaImage; });
    }
    for (let key in removed_pipes) {
      pipes[key].remove();
      delete pipes[key];
    }
    for (let key in updated_pipes) {
      // TODO(fairlight1337): Figure out what to update exactly once that is implemented in the
      // backend.
    }
    // Process model metadata. The backend reports `"metadata": null` for pipelines that
    // have never had any UI state persisted, so we defend against the non-object case
    // before doing any `in` / property access (`"pipes" in null` throws a TypeError, which
    // is what used to white-screen the entire app the moment you opened a fresh pipeline).
    const safeMetadata = (metadata && typeof metadata === 'object') ? metadata : {};
    if (safeMetadata["pipes"]) {
      for (let pipe_id in safeMetadata["pipes"]) {
        const pipeMeta = safeMetadata["pipes"][pipe_id];
        if (pipeMeta && pipeMeta["position"] && pipe_id in pipes &&
            currentlyDraggedPipe != pipe_id) {
          pipes[pipe_id].x(pipeMeta["position"]["x"]);
          pipes[pipe_id].y(pipeMeta["position"]["y"]);
        }
      }
    }
  }, [ model ]);

  useEffect(() => {
    // Process connections. The stage useEffect runs before this one on first mount, but
    // we still defend against a missing ref in case React schedules things unusually under
    // concurrent mode.
    if (!connectionsLayerRef.current) return;
    connectionsLayerRef.current.removeChildren();
    const conns = (model && model["connections"]) ? model["connections"] : null;
    if (conns) {
      for (let connection_index in conns) {
        const connection = conns[connection_index];
        if (!connection || !(connection.source_pipe in pipes) ||
            !(connection.destination_pipe in pipes)) {
          continue;
        }
        const source_pipe = pipes[connection.source_pipe];
        const destination_pipe = pipes[connection.destination_pipe];
        const source_port = source_pipe.ports.outputs[connection.source_slot];
        const destination_port = destination_pipe.ports.inputs[connection.destination_slot];
        if (!source_port || !destination_port) continue;

        const line = new Konva.Line({
          points : [
            source_pipe.x() + source_port.x() + 1.5, source_pipe.y() + source_port.y() + 5,
            destination_pipe.x() + destination_port.x(),
            destination_pipe.y() + destination_port.y() + 5
          ],
          stroke : 'red',
          strokeWidth : 2,
          lineCap : 'round',
          lineJoin : 'round',
          // Stash the connection metadata directly on the Konva object so the click
          // handler doesn't have to search the model again.
          hitStrokeWidth: 10,
        });
        line.connectionInfo = {
          source_pipe: connection.source_pipe,
          source_slot: connection.source_slot,
          destination_pipe: connection.destination_pipe,
          destination_slot: connection.destination_slot,
        };
        line.on('mouseover', () => {
          line.stroke('#ff8c00');
          line.getLayer().batchDraw();
        });
        line.on('mouseout', () => {
          line.stroke('red');
          line.getLayer().batchDraw();
        });
        // Right-click on a connection: queue it for delete (the ConfirmationDialog at the
        // bottom of the render asks before actually mutating).
        line.on('contextmenu', (event) => {
          event.evt.preventDefault();
          event.evt.stopPropagation();
          event.cancelBubble = true;
          setPendingDeleteConnection(line.connectionInfo);
        });
        connectionsLayerRef.current.add(line);
      }
    }
    // While a connection is being authored, draw a guide line from the source port to
    // wherever the mouse last hovered. This makes the gesture feel responsive instead of
    // requiring the user to remember which port they clicked first.
    if (pendingConnectionSource) {
      const sourcePipe = pipes[pendingConnectionSource.pipeName];
      const sourcePort = sourcePipe && sourcePipe.ports.outputs[pendingConnectionSource.slot];
      if (sourcePort) {
        const sx = sourcePipe.x() + sourcePort.x() + 1.5;
        const sy = sourcePipe.y() + sourcePort.y() + 5;
        const guide = new Konva.Line({
          points: [sx, sy, sx, sy],
          stroke: '#ff8c00',
          strokeWidth: 2,
          dash: [4, 4],
          lineCap: 'round',
          listening: false,
        });
        guide.name('connection-guide');
        connectionsLayerRef.current.add(guide);
      }
    }
    connectionsLayerRef.current.draw();
  }, [ dragMovePoint, model, pendingConnectionSource ]);

  // Track mouse moves at the stage level so the guide line follows the cursor while the
  // user is choosing the destination port. Cheap because we just mutate the existing line;
  // no React re-render happens here.
  useEffect(() => {
    if (!stageInstance) return;
    const handler = () => {
      if (!pendingConnectionSourceRef.current || !connectionsLayerRef.current) return;
      const layer = connectionsLayerRef.current;
      const guides = layer.find('.connection-guide');
      if (!guides || guides.length === 0) return;
      const pos = stageInstance.getPointerPosition();
      if (!pos) return;
      const elementLayer = elementLayerRef.current;
      const offsetX = elementLayer ? elementLayer.x() : 0;
      const offsetY = elementLayer ? elementLayer.y() : 0;
      const guide = guides[0];
      const startPoints = guide.points();
      guide.points([startPoints[0], startPoints[1], pos.x - offsetX, pos.y - offsetY]);
      layer.batchDraw();
    };
    stageInstance.on('mousemove', handler);
    return () => { stageInstance.off('mousemove', handler); };
  }, [ stageInstance, pendingConnectionSource ]);

  // Esc cancels an in-progress connection-authoring gesture.
  useEffect(() => {
    const onKey = (e) => {
      if (e.key === 'Escape' && pendingConnectionSourceRef.current) {
        cancelPendingConnection();
      }
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, []);

  // Update the grid and border when dimensions change
  const updateGridAndBorder = () => {
    gridLayerRef.current.destroyChildren();
    drawGrid(gridLayerRef.current, dimensions.width, dimensions.height, 20,
             3); // Grid size = 20, gridSizeMultiplier = 3
    gridLayerRef.current.batchDraw();

    borderLayerRef.current.destroyChildren();
    drawBorder(borderLayerRef.current, dimensions.width, dimensions.height);
    borderLayerRef.current.batchDraw();
  };

  useEffect(() => {
    if (stageInstance) {
      stageInstance.width(dimensions.width);
      stageInstance.height(dimensions.height);
      stageInstance.batchDraw();
    }

    // Set up a listener for dimension changes
    const onDimensionsChange = () => {
      if (!stageInstance)
        return;
      stageInstance.width(dimensions.width);
      stageInstance.height(dimensions.height);
      updateGridAndBorder();
    };

    // Update the grid and border initially
    updateGridAndBorder();

    // Update the grid and border when dimensions change
    const dimensionsObserver = new ResizeObserver(onDimensionsChange);
    dimensionsObserver.observe(stageRef.current);

    return () => { dimensionsObserver.disconnect(); };
  }, [ dimensions ]);

  useResize(() => {
    setDimensions({width : window.innerWidth - 320, height : window.innerHeight - 200});
  });

  const handleButtonClick = async (id, action) => {
    const response = await fetch(`/api/v1/pipelines/${id}/${action}`, {
      method : "GET",
    });
    // Handle response if necessary
  };

  useEffect(() => {
    const controller = new AbortController();
    const fetchPipelineState = async () => {
      try {
        const response = await fetch(`/api/v1/pipelines/${pipeline.id}`,
                                     {signal : controller.signal});
        if (!response.ok) throw new Error('not ok');
        const jsonData = await response.json();
        pipelineStateRef.current = jsonData.state;
        setPipelineState(jsonData.state);
        setModel(jsonData.model || {});
        setMetadata(jsonData.metadata || {});
      } catch (e) {
        // Network blip or unmount; ignore and try again on the next tick.
      }
    };
    // Expose the fetcher to non-effect callers (the API mutation helper) so they can force
    // a refresh after a successful POST instead of waiting for the next poll cycle.
    refreshPipelineStateRef.current = fetchPipelineState;
    fetchPipelineState();
    const interval = setInterval(fetchPipelineState, 1000);
    return () => {
      clearInterval(interval);
      controller.abort();
      refreshPipelineStateRef.current = () => {};
    };
  }, [ pipeline.id ]);

  useEffect(() => {
    const controller = new AbortController();
    const fetchMetrics = async () => {
      try {
        const response = await fetch(`/api/v1/pipelines/${pipeline.id}/metrics`,
                                     {signal : controller.signal});
        if (!response.ok) throw new Error('not ok');
        const jsonData = await response.json();
        setMetrics(jsonData);
      } catch (e) {
        // Network blip or unmount; ignore.
      }
    };
    // 100ms used to be the cadence; 500ms is still smooth and easier on the backend.
    fetchMetrics();
    const interval = setInterval(fetchMetrics, 500);
    return () => {
      clearInterval(interval);
      controller.abort();
    };
  }, [ pipeline.id ]);

  useEffect(() => {
    if (stageInstance) {
      stageInstance.width(dimensions.width);
      stageInstance.height(dimensions.height);
      stageInstance.batchDraw();
    }
  }, [ dimensions, stageInstance ]);

  const resetGridPosition = () => {
    if (gridLayerRef.current) {
      gridLayerRef.current.x(-dimensions.width);  // Reset X position
      gridLayerRef.current.y(-dimensions.height); // Reset Y position
      gridLayerRef.current.batchDraw();
    }

    elementLayerRef.current.x(0);
    elementLayerRef.current.y(0);
  };

  const handleOpenRenameDialog = () => {
    setNewPipelineName(pipeline.name);
    setRenameDialogOpen(true);
  };

  const handleSaveRenameDialog = async () => {
    try {
      const response = await fetch(`/api/v1/pipelines/${pipeline.id}/update`, {
        method : "POST",
        headers : {
          "Content-Type" : "application/json",
        },
        body : JSON.stringify({action : "change_name", name : newPipelineName.trim()}),
      });
      // Handle response if necessary
    } catch (error) {
      // Handle error if necessary
    }
    setRenameDialogOpen(false);
    setPipelineName(newPipelineName.trim());
  };

  return e(
      React.Fragment,
      null,
      e(Toolbar, {className : classes.toolbar},
        e(IconButton, {
          edge : "start",
          color : "inherit",
          onClick : onBackButtonClick,
        },
          e("i", {className : "material-icons"}, "arrow_back")),
        e(Box, {
          className : classes.pipelineName,
          style : {flexGrow : 1},
          onMouseEnter : () => setShowEditButton(true),
          onMouseLeave : () => setShowEditButton(false),
        },
          e(Typography, {display : "inline"}, pipelineName),
          showEditButton &&
              e(IconButton, {
                edge : "end",
                color : "inherit",
                onClick : handleOpenRenameDialog,
                style : {padding : 3, marginLeft : 4},
              },
                e("i", {className : "material-icons", style : {fontSize : 18}}, "create"))),
        e(
            "div",
            null,
            e(Button, {
              variant : "contained",
              color : "default",
              onClick : resetGridPosition,
            },
              e("i", {className : "material-icons"}, "home")),
            e(Button, {
              variant : "contained",
              style : {
                backgroundColor : pipelineState === "running" ? "red" : "green",
                color : "white",
              },
              onClick : () =>
                  handleButtonClick(pipeline.id, pipelineState === "running" ? "stop" : "start"),
            },
              pipelineState === "running" ? e("i", {className : "material-icons"}, "stop")
                                          : e("i", {className : "material-icons"}, "play_arrow")),
            )),
      e("div", {
        style : {width : "100%", height : "calc(100% - 64px)", position : "relative"},
        ref : stageRef
      },
        e(ContextMenu, {
          show : showContextMenu,
          position : contextMenuPosition,
          onClose : () => setShowContextMenu(false),
          menuItems : rightClickMenuItemsRef.current,
        })),
      e(Dialog, {open : renameDialogOpen, onClose : () => setRenameDialogOpen(false)},
        e(DialogTitle, null, "Edit Pipeline Title"),
        e(DialogContent, null, e(TextField, {
            autoFocus : true,
            margin : "dense",
            label : "Pipeline Title",
            value : newPipelineName,
            onChange : (event) => setNewPipelineName(event.target.value),
            fullWidth : true,
          })),
        e(DialogActions, null, e(Button, {onClick : () => setRenameDialogOpen(false)}, "Cancel"),
          e(Button, {
            onClick : handleSaveRenameDialog,
            disabled : !newPipelineName.trim() || newPipelineName.trim() === pipeline.name,
          },
            "Save"))),
      // Modal for adding a new pipe. The dialog itself only knows how to render its form
      // and produce a request body; the actual mutation goes through `postUpdate` so the
      // error-toast and refresh logic stay in one place.
      e(AddPipeDialog, {
        open: addPipeOpen,
        anchorPosition: addPipeAnchor,
        existingPipeNames: Object.keys((model && model.pipes) || {}),
        editing: editingPipe,
        onClose: () => {
          setAddPipeOpen(false);
          setEditingPipe(null);
        },
        onSave: async (body) => {
          const ok = await postUpdate(body);
          if (ok) {
            setAddPipeOpen(false);
            setEditingPipe(null);
          }
          return ok;
        },
      }),
      // Delete confirmations -- one for pipes, one for connections. Both route through
      // ConfirmationDialog so the wording reads consistently and the same Yes/No buttons
      // appear in both places.
      e(ConfirmationDialog, {
        open: Boolean(pendingDelete),
        onClose: () => setPendingDelete(null),
        onConfirm: async () => {
          await postUpdate({action: 'delete_pipe', name: pendingDelete});
          setPendingDelete(null);
        },
        title: 'Delete pipe?',
        content: pendingDelete
          ? `Delete pipe "${pendingDelete}"? Any connections to or from it will also be removed.`
          : '',
      }),
      e(ConfirmationDialog, {
        open: Boolean(pendingDeleteConnection),
        onClose: () => setPendingDeleteConnection(null),
        onConfirm: async () => {
          await postUpdate({
            action: 'delete_connection',
            ...pendingDeleteConnection,
          });
          setPendingDeleteConnection(null);
        },
        title: 'Delete connection?',
        content: pendingDeleteConnection
          ? `Disconnect ${pendingDeleteConnection.source_pipe}[${pendingDeleteConnection.source_slot}] → ${pendingDeleteConnection.destination_pipe}[${pendingDeleteConnection.destination_slot}]?`
          : '',
      }),
      // Top-right floating panel listing every ResultsSummaryPipe in the pipeline with
      // its live stats. This is the primary place to see "how is the maze doing?" without
      // having to hover over each pipe individually. Each entry exposes a Reset button
      // that hits the `reset_summary` action so the user can clear the rolling window
      // after bumping the maze difficulty.
      (() => {
        const pipesMetrics = metrics && Array.isArray(metrics["pipes"]) ? metrics["pipes"] : [];
        const summaryPipes = pipesMetrics.filter(p => p && p.summary);
        if (summaryPipes.length === 0) return null;
        const fmt = (v) => Number.isFinite(v) ? Number(v).toFixed(3) : '--';
        return e('div', {
          style: {
            position: 'absolute',
            right: 16,
            top: 16,
            padding: '10px 12px',
            backgroundColor: 'rgba(255,255,255,0.95)',
            border: '1px solid #ccc',
            borderRadius: 4,
            boxShadow: '0 2px 6px rgba(0,0,0,0.12)',
            zIndex: 10,
            maxWidth: 320,
            fontFamily: 'sans-serif',
            fontSize: 13,
          },
        },
          e('div', {style: {fontWeight: 'bold', marginBottom: 6}}, 'Results summaries'),
          ...summaryPipes.map((p, idx) =>
            e('div', {
              key: 'sp' + p.name,
              style: {
                paddingTop: idx === 0 ? 0 : 6,
                marginTop: idx === 0 ? 0 : 6,
                borderTop: idx === 0 ? 'none' : '1px solid #eee',
              },
            },
              e('div', {style: {fontWeight: 'bold'}}, p.name),
              e('div', null, `seen: ${p.summary.count_total} (window ${p.summary.count_window}/${p.summary.window_size || '--'})`),
              e('div', null, `min/mean/max: ${fmt(p.summary.min_score)} / ${fmt(p.summary.mean_score)} / ${fmt(p.summary.max_score)}`),
              e('div', null, `last: ${fmt(p.summary.last_score)}`),
              e('div', null, `best ever: ${fmt(p.summary.best_ever_score)} (${p.summary.best_ever_size} bytes)`),
              e('button', {
                onClick: () => postUpdate({action: 'reset_summary', name: p.name}),
                style: {
                  marginTop: 4,
                  padding: '2px 8px',
                  fontSize: 12,
                  cursor: 'pointer',
                },
                title: 'Reset rolling window and best-ever (use after bumping difficulty)',
              }, 'Reset'),
            )),
        );
      })(),

      // Status bar for the in-flight connection authoring gesture, plus any API errors.
      // Both render as small floating banners inside the canvas viewport so the user gets
      // immediate feedback without us having to wire a separate notification system.
      pendingConnectionSource &&
          e('div', {
            style: {
              position: 'absolute',
              left: 16,
              bottom: 16,
              padding: '8px 12px',
              backgroundColor: '#fff3cd',
              border: '1px solid #ffeeba',
              borderRadius: 4,
              boxShadow: '0 2px 4px rgba(0,0,0,0.1)',
              zIndex: 10,
            },
          },
            `Click a destination input port to finish the connection (or press Esc to cancel). Source: ${pendingConnectionSource.pipeName}[${pendingConnectionSource.slot}]`),
      apiError &&
          e('div', {
            style: {
              position: 'absolute',
              right: 16,
              bottom: 16,
              padding: '8px 12px',
              backgroundColor: '#f8d7da',
              color: '#721c24',
              border: '1px solid #f5c6cb',
              borderRadius: 4,
              boxShadow: '0 2px 4px rgba(0,0,0,0.1)',
              zIndex: 10,
              maxWidth: 360,
              cursor: 'pointer',
            },
            onClick: () => setApiError(''),
            title: 'Click to dismiss',
          },
            apiError),
      // Pipe-info tooltip: rendered through a portal directly into <body> so it floats above
      // the Konva canvas without being clipped by Material UI's overflow rules. Only shows
      // when a pipe is hovered and the right-click menu isn't currently up.
      (hoveredPipe && !showContextMenu)
          ? createPortal(e(PipeInfoTooltip,
                           {hoveredPipe : hoveredPipe, x : tooltipPos.x, y : tooltipPos.y}),
                         document.body)
          : null,
  );
}

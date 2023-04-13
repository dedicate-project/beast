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
const {Stage, Layer, Rect} = Konva;

import {ContextMenu} from './ContextMenu.js';

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

const onResize = () => { callback(); };

const useResize = (callback) => {
  useEffect(() => {
    window.addEventListener("resize", onResize);
    return () => window.removeEventListener("resize", onResize);
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
        e.evt.preventDefault();
        const position = stage.getPointerPosition();
        setContextMenuPosition({x : position.x, y : position.y});
        setShowContextMenu(true);
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
    const createDraggableImage = async (src, x, y, inports, outports) => {
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
          x : (item.width() - 50) / 2,
          y : (item.height() - 50) / 2,
          image : imageObj,
          width : 50,
          height : 50,
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

      // add event listeners to the Rect object
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
    var added_pipes = {};
    var updated_pipes = {};
    var removed_pipes = {};
    for (let key in model["pipes"]) {
      // Check if in old model
      if ("pipes" in oldModel) {
        if (key in oldModel["pipes"]) {
          // Was in old model
          if (model["pipes"][key] != oldModel["pipes"][key]) {
            // Update
            updated_pipes[key] = model["pipes"][key];
          } else {
            // Ignore
          }
        } else {
          // Was not in old model
          added_pipes[key] = model["pipes"][key];
        }
      } else {
        added_pipes[key] = model["pipes"][key];
      }
    }
    for (let key in oldModel["pipes"]) {
      // Check if in new model
      if (key in model["pipes"]) {
        // Is in new model. Ignore.
      } else {
        // Is not in new model. This was removed.
        removed_pipes[key] = model["pipes"][key];
      }
    }
    setOldModel(model);

    for (let key in added_pipes) {
      var inports = 0;
      var outports = 0;
      let image_file = "/img/pipe_plain_oneinputoneoutput.png";
      if (added_pipes[key]["type"] == "ProgramFactoryPipe") {
        image_file = "/img/factory_pipe.png";
        outports = 1;
      } else if (added_pipes[key]["type"] == "NullSinkPipe") {
        image_file = "/img/null_sink_pipe.png";
        inports = 1;
      } else if (added_pipes[key]["type"] == "EvaluatorPipe") {
        if (added_pipes[key]["parameters"]["evaluators"][0]["type"] == "MazeEvaluator") {
          image_file = "/img/pipe_maze.png";
          inports = 1;
          outports = 1;
        }
      }
      var pos_x = 50;
      var pos_y = 50;
      if ("pipes" in metadata && key in metadata["pipes"] && "position" in metadata["pipes"][key]) {
        pos_x = metadata["pipes"][key]["position"]["x"];
        pos_y = metadata["pipes"][key]["position"]["y"];
      }

      createDraggableImage(image_file, pos_x, pos_y, inports, outports)
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
    // Process model metadata
    if ("pipes" in metadata) {
      for (let pipe_id in metadata["pipes"]) {
        if ("position" in metadata["pipes"][pipe_id]) {
          if (pipe_id in pipes && currentlyDraggedPipe != pipe_id) {
            pipes[pipe_id].x(metadata["pipes"][pipe_id]["position"]["x"]);
            pipes[pipe_id].y(metadata["pipes"][pipe_id]["position"]["y"]);
          }
        }
      }
    }
  }, [ model ]);

  useEffect(() => {
    // Process connections
    connectionsLayerRef.current.removeChildren();
    if ("connections" in model) {
      for (let connection_index in model["connections"]) {
        const connection = model["connections"][connection_index];
        if (!(connection.source_pipe in pipes) || !(connection.destination_pipe in pipes)) {
          continue;
        }
        const source_pipe = pipes[connection.source_pipe];
        const destination_pipe = pipes[connection.destination_pipe];
        const source_port = source_pipe.ports.outputs[connection.source_slot];
        const destination_port = destination_pipe.ports.inputs[connection.destination_slot];

        const line = new Konva.Line({
          points : [
            source_pipe.x() + source_port.x() + 1.5, source_pipe.y() + source_port.y() + 5,
            destination_pipe.x() + destination_port.x(),
            destination_pipe.y() + destination_port.y() + 5
          ],
          stroke : 'red',
          strokeWidth : 2,
          lineCap : 'round',
          lineJoin : 'round'
        });
        connectionsLayerRef.current.add(line);
      }
    }
    connectionsLayerRef.current.draw();
  }, [ dragMovePoint, model ]);

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

  const fetchPipelineState = async () => {
    try {
      const response = await fetch(`/api/v1/pipelines/${pipeline.id}`);
      if (!response.ok) {
        throw new Error('Network response was not ok');
      }
      const jsonData = await response.json();
      setPipelineState(jsonData.state);
      console.log(jsonData.model);
      setModel(jsonData.model);
      setMetadata(jsonData.metadata);
    } catch (error) {
      console.error('Error fetching pipeline state:', error);
    }
  };

  useEffect(() => {
    fetchPipelineState();                                   // Fetch the pipeline state initially
    const interval = setInterval(fetchPipelineState, 1000); // Fetch the pipeline state every 1000ms
    return () => clearInterval(interval); // Cleanup the interval on component unmount
  }, []);

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
          menuItems : [ "Item 1", "Item 2", "Item 3" ],
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
  );
}

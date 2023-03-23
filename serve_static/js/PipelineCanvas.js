const { useState, createElement: e, useEffect, useRef, useLayoutEffect } = React;
const { Typography, Box, Button, TextField, Toolbar, IconButton, makeStyles } = MaterialUI;
const { Stage, Layer, Rect } = Konva;

const useStyles = makeStyles((theme) => ({
  toolbar: {
    display: "flex",
    justifyContent: "space-between",
    marginBottom: theme.spacing(2),
    alignItems: "center",
  },
  pipelineName: {
    fontWeight: "bold",
    fontSize: "1.5rem",
    marginLeft: theme.spacing(1),
  },
}));

const useResize = (callback) => {
  const onResize = () => {
    callback();
  };

  useEffect(() => {
    window.addEventListener("resize", onResize);
    return () => window.removeEventListener("resize", onResize);
  }, []);
};

export function PipelineCanvas({ pipeline, onBackButtonClick }) {
  const classes = useStyles();
  const [pipelineState, setPipelineState] = useState(pipeline.state);
  const [dimensions, setDimensions] = useState({ width: window.innerWidth - 320, height: window.innerHeight - 200 });
  const [stageInstance, setStageInstance] = useState(null);

  const stageRef = React.useRef(null);

  React.useEffect(() => {
    const stage = new Konva.Stage({
      container: stageRef.current, // Attach the stage to the div with ref 'stageRef'
      width: dimensions.width,
      height: dimensions.height,
    });
    
    setStageInstance(stage); // Store the stage instance in the state variable

    const layer = new Konva.Layer();
    stage.add(layer);

    const rect = new Konva.Rect({
      x: 20,
      y: 20,
      width: 100,
      height: 100,
      fill: 'green',
      draggable: true,
    });
    layer.add(rect);

    layer.draw();
  }, []);

  useResize(() => {
    setDimensions({ width: window.innerWidth - 320, height: window.innerHeight - 200 });
  });

  const handleButtonClick = async (id, action) => {
    const response = await fetch(`/api/v1/pipelines/${id}/${action}`, {
      method: "GET",
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
    } catch (error) {
      console.error('Error fetching pipeline state:', error);
    }
  };

  useEffect(() => {
    fetchPipelineState(); // Fetch the pipeline state initially
    const interval = setInterval(fetchPipelineState, 1000); // Fetch the pipeline state every 1000ms
    return () => clearInterval(interval); // Cleanup the interval on component unmount
  }, []);
  
  useEffect(() => {
    if (stageInstance) {
      stageInstance.width(dimensions.width);
      stageInstance.height(dimensions.height);
      stageInstance.batchDraw();
    }
  }, [dimensions, stageInstance]);

  return e(
    React.Fragment,
    null,
    e(
      Toolbar,
      { className: classes.toolbar },
      e(
        IconButton,
        {
          edge: "start",
          color: "inherit",
          onClick: onBackButtonClick,
        },
        e("i", { className: "material-icons" }, "arrow_back")
      ),
      e(
        Typography,
        {
          className: classes.pipelineName,
          style: { flexGrow: 1 },
        },
        pipeline.name
      ),
      e(
        "div",
        null,
        e(Button, {
          variant: "contained",
          style: {
            backgroundColor: pipelineState === "running" ? "red" : "green",
            color: "white",
          },
          onClick: () => handleButtonClick(pipeline.id, pipelineState === "running" ? "stop" : "start"),
        }, pipelineState === "running" ? e("i", { className: "material-icons" }, "stop") : e("i", { className: "material-icons" }, "play_arrow"))
      )
    ),
    e(
      "div",
      { style: { width: "100%", height: "calc(100% - 64px)", position: "relative" },
        ref: stageRef },
    )
  );
}

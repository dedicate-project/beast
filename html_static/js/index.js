import App from './App.js';

const rootElement = document.getElementById('root');
const appElement = React.createElement(App, null);

ReactDOM.createRoot(rootElement).render(React.createElement(React.StrictMode, null, appElement));

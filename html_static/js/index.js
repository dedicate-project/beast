import App from './App.js';
import {ErrorBoundary} from './ErrorBoundary.js';

const rootElement = document.getElementById('root');
const tree = React.createElement(React.StrictMode, null,
                                 React.createElement(ErrorBoundary, null,
                                                     React.createElement(App, null)));

ReactDOM.createRoot(rootElement).render(tree);

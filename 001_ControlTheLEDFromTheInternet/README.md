# Arduino LED Web Control (Free Hosting)

This project gives you:
- A web page with `ON` and `OFF` buttons
- An API endpoint your Arduino can poll

## API

- `POST /api/login` with body `{ "username": "...", "password": "..." }` -> `{ "token": "..." }`
- `GET /api/led` -> `{ "state": "on" }` or `{ "state": "off" }`
- `POST /api/led` with body `{ "state": "on" }` or `{ "state": "off" }`

`/api/led` now requires `Authorization: Bearer <token>`.
Default credentials are:
- username: `admin`
- password: `admin123`

Set custom credentials in hosting environment variables:
- `WEB_USERNAME`
- `WEB_PASSWORD`

## Run locally

```bash
npm install
npm start
```

Then open: `http://localhost:3000`

## Deploy free on Render

1. Create a GitHub repo and push these files.
2. Go to [Render](https://render.com/) and create a free account.
3. Click **New +** -> **Web Service**
4. Connect your GitHub repo.
5. Use these settings:
   - Runtime: `Node`
   - Build Command: `npm install`
   - Start Command: `npm start`
6. Deploy.

After deploy, you will get a URL like:
`https://your-app-name.onrender.com`

Use this URL in Arduino:
- LED state endpoint: `https://your-app-name.onrender.com/api/led`

## Important note

This prototype stores LED state in server memory (in `server.js`).
It is perfect for testing and demos. If the service restarts, state resets to `off`.

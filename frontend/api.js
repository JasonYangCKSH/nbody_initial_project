const BACKEND_HOSTS = [
  "http://localhost:3000",
  "http://127.0.0.1:3000",
];

async function fetchWithBackend(path, options = {}) {
  const requestOptions = {
    ...options,
    mode: "cors",
    cache: "no-cache",
  };

  for (const host of BACKEND_HOSTS) {
    try {
      const res = await fetch(`${host}${path}`, requestOptions);
      if (res.ok) return res;
      const text = await res.text().catch(() => "");
      throw new Error(`Request to ${host}${path} failed: ${res.status} ${text}`);
    } catch (err) {
      // try next host
      console.warn(`Backend fetch failed for ${host}${path}:`, err.message || err);
    }
  }

  throw new Error(`All backend fetch attempts failed for ${path}`);
}

window.fetchConfig = async function () {
  const res = await fetchWithBackend("/api/config");
  return res.json();
};

window.stepSimulation = async function (payload) {
  const res = await fetchWithBackend("/api/step", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
    },
    body: JSON.stringify(payload),
  });
  return res.json();
};

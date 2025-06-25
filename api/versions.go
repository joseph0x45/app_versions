package api

import (
	"encoding/json"
	"net/http"
)

type Response struct {
	LatestVersion string `json:"latest_version"`
	DownloadURL   string `json:"download_url"`
}

// GET /api/versions
func Handler(w http.ResponseWriter, r *http.Request) {
	queryParams := r.URL.Query()
	targetApp := queryParams.Get("name")
	response := &Response{
		LatestVersion: targetApp,
		DownloadURL:   "http://somelink",
	}
	jsonData, err := json.Marshal(response)
	if err != nil {
		w.WriteHeader(http.StatusInternalServerError)
		w.Write([]byte("Something went bad"))
		return
	}
	w.WriteHeader(http.StatusOK)
	w.Write(jsonData)
}

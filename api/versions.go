package api

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strings"
)

type Response struct {
	LatestVersion string `json:"latest_version"`
	DownloadURL   string `json:"download_url"`
}

type Asset struct {
	URL                string `json:"url"`
	Name               string `json:"name"`
	BrowserDownloadURL string `json:"browser_download_url"`
}

type Release struct {
	TagName     string  `json:"tag_name"`
	AppImageURL string  `json:"app_image_url"`
	Assets      []Asset `json:"assets"`
}

func getGithubLatestRelease(appName string) *Release {
	appToURL := map[string]string{
		"zen":   "https://api.github.com/repos/zen-browser/desktop/releases/latest",
		"teams": "https://api.github.com/repos/IsmaelMartinez/teams-for-linux/releases/latest",
	}
	req, err := http.NewRequest("GET", appToURL[appName], nil)
	if err != nil {
		fmt.Printf("Error while creating request: %s", err.Error())
		return nil
	}
	res, err := http.DefaultClient.Do(req)
	if err != nil {
		fmt.Printf("Error while getting release: %s", err.Error())
		return nil
	}
	body, err := io.ReadAll(res.Body)
	if err != nil {
		fmt.Printf("Error while reading response body: %s", err.Error())
		return nil
	}
	if res.StatusCode != 200 {
		fmt.Printf("Expected HTTP 200 but got %d; Error %s", res.StatusCode, string(body))
		return nil
	}
	release := &Release{}
	err = json.Unmarshal(body, release)
	if err != nil {
		fmt.Printf("Error while unmarshalling data: %s", err.Error())
		return nil
	}
	return release
}

func getDownloadName(appName string, appVersion string) string {
	if appName == "zen" {
		return "zen-x86_64.AppImage"
	}
	appVersion = strings.TrimPrefix(appVersion, "v")
	return fmt.Sprintf("teams-for-linux-%s-AppImage", appVersion)
}

// GET /api/versions
func Handler(w http.ResponseWriter, r *http.Request) {
	queryParams := r.URL.Query()
	appName := queryParams.Get("name")
	if appName != "zen" && appName != "teams" {
		w.WriteHeader(http.StatusBadRequest)
		return
	}
	release := getGithubLatestRelease(appName)
	if release == nil {
		w.WriteHeader(http.StatusInternalServerError)
		return
	}
	downloadName := getDownloadName(appName, release.TagName)
	for _, asset := range release.Assets {
		if asset.Name == downloadName {
			release.AppImageURL = asset.BrowserDownloadURL
			break
		}
	}
	data, _ := json.Marshal(release)
	w.WriteHeader(http.StatusOK)
	w.Header().Set("Content-Type", "application/json")
	w.Write(data)
}

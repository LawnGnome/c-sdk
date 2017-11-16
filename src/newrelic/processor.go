package newrelic

import (
	"encoding/json"
	"strings"
	"time"

	"newrelic/collector"
	"newrelic/log"
	"newrelic/utilization"
)

type TxnData struct {
	ID     AgentRunID
	Sample AggregaterInto
}

type AppInfoReply struct {
	RunIDValid   bool
	State        AppState
	ConnectReply []byte
}

type AppInfoMessage struct {
	ID         *AgentRunID
	Info       *AppInfo
	ResultChan chan AppInfoReply
}

type ConnectAttempt struct {
	Key       AppKey
	Collector string
	Reply     *ConnectReply
	RawReply  []byte
	Err       error
}

type HarvestError struct {
	Err   error
	id    AgentRunID
	Reply []byte
	data  FailedHarvestSaver
}

type HarvestType uint8

const (
	HarvestMetrics      HarvestType = (1 << 0)
	HarvestErrors       HarvestType = (1 << 1)
	HarvestSlowSQLs     HarvestType = (1 << 2)
	HarvestTxnTraces    HarvestType = (1 << 3)
	HarvestTxnEvents    HarvestType = (1 << 4)
	HarvestCustomEvents HarvestType = (1 << 5)
	HarvestErrorEvents  HarvestType = (1 << 6)
	HarvestDefaultData  HarvestType = HarvestMetrics | HarvestErrors | HarvestSlowSQLs | HarvestTxnTraces
	HarvestAll          HarvestType = HarvestDefaultData | HarvestTxnEvents | HarvestCustomEvents | HarvestErrorEvents
)

// This type represents a processor harvest event: when this is received by a
// processor, it indicates that a harvest should be performed for the harvest
// and run ID contained within. The word "event" doesn't appear in the type only
// to avoid confusion with analytic events.
type ProcessorHarvest struct {
	*AppHarvest
	ID   AgentRunID
	Type HarvestType
}

type ProcessorConfig struct {
	UseSSL          bool
	Client          collector.Client
	IntegrationMode bool
	UtilConfig      utilization.Config
	AppTimeout      time.Duration
}

type Processor struct {
	// This map contains all applications, even those that are permanently
	// disconnected or have invalid license keys.
	apps map[AppKey]*App
	// This map contains only connected applications.
	harvests map[AgentRunID]*AppHarvest

	txnDataChannel        chan TxnData
	appInfoChannel        chan AppInfoMessage
	connectAttemptChannel chan ConnectAttempt
	harvestErrorChannel   chan HarvestError
	quitChan              chan struct{}
	processorHarvestChan  chan ProcessorHarvest
	trackProgress         chan struct{} // Usually nil, used for testing
	appConnectBackoff     time.Duration
	cfg                   ProcessorConfig
	util                  *utilization.Data
}

func (p *Processor) processTxnData(d TxnData) {
	// First make sure the agent run id is valid
	h, ok := p.harvests[d.ID]
	if !ok {
		log.Debugf("bad TxnData: run id no longer valid: %s", d.ID)
		return
	}

	h.Harvest.commandsProcessed++
	h.App.LastActivity = time.Now()
	d.Sample.AggregateInto(h.Harvest)
}

type ConnectArgs struct {
	RedirectCollector string
	Payload           []byte
	License           collector.LicenseKey
	UseSSL            bool
	Client            collector.Client
	AppKey            AppKey
}

func ConnectApplication(args *ConnectArgs) ConnectAttempt {
	rep := ConnectAttempt{Key: args.AppKey}

	call := collector.Cmd{
		Name:      collector.CommandRedirect,
		UseSSL:    args.UseSSL,
		Collector: args.RedirectCollector,
		License:   args.License,
		Collectible: collector.CollectibleFunc(func(auditVersion bool) ([]byte, error) {
			return []byte("[]"), nil
		}),
	}

	out, err := args.Client.Execute(call)
	if nil != err {
		rep.Err = err
		return rep
	}

	rep.Err = json.Unmarshal(out, &rep.Collector)
	if nil != rep.Err {
		return rep
	}

	call.Collector = rep.Collector
	call.Collectible = collector.CollectibleFunc(func(auditVersion bool) ([]byte, error) {
		return args.Payload, nil
	})

	call.Name = collector.CommandConnect

	rep.RawReply, rep.Err = args.Client.Execute(call)
	if nil != rep.Err {
		return rep
	}

	processConnectMessages(rep.RawReply)

	rep.Reply, rep.Err = parseConnectReply(rep.RawReply)

	return rep
}

func (p *Processor) shutdownAppHarvest(id AgentRunID) {
	if nil != p.harvests[id] {
		p.harvests[id].Close()
		delete(p.harvests, id)
	}
}

func (p *Processor) shouldConnect(app *App, now time.Time) bool {
	if p.util == nil {
		return false
	}

	if !app.NeedsConnectAttempt(now, p.appConnectBackoff) {
		return false
	}
	return true
}

func (p *Processor) considerConnect(app *App) {
	now := time.Now()
	if !p.shouldConnect(app, now) {
		return
	}
	app.lastConnectAttempt = now

	data, err := app.info.ConnectJSON(p.util)
	if err != nil {
		log.Errorf("unable to connect application: %v", err)
		return
	}

	args := &ConnectArgs{
		RedirectCollector: app.info.RedirectCollector,
		Payload:           data,
		License:           app.info.License,
		UseSSL:            p.cfg.UseSSL,
		Client:            p.cfg.Client,
		AppKey:            app.Key(),
	}

	go func() {
		p.connectAttemptChannel <- ConnectApplication(args)
	}()
}

func (p *Processor) processAppInfo(m AppInfoMessage) {
	var app *App
	r := AppInfoReply{State: AppStateUnknown}

	defer func() {
		if nil != app {
			r.State = app.state
			if AppStateConnected == app.state {
				r.ConnectReply = app.RawConnectReply
			}
		}
		// Send the response back before attempting to connect the application
		m.ResultChan <- r
		if nil != app {
			p.considerConnect(app)
		}
	}()

	if !p.cfg.UseSSL && m.Info.HighSecurity {
		log.Warnf("unable to add high security app '%s', daemon not configured to use ssl", m.Info)
		return
	}

	if nil != m.ID {
		if _, ok := p.harvests[*m.ID]; ok {
			r.RunIDValid = true
			return
		}
		// This agent run id must be out of date, fall through:
	}

	key := m.Info.Key()
	app = p.apps[key]
	if nil != app {
		return
	}

	if len(p.apps) > AppLimit {
		log.Errorf("unable to add app '%s', limit of %d applications reached",
			m.Info, AppLimit)
		return
	}

	app = NewApp(m.Info)
	p.apps[key] = app
}

func processConnectMessages(reply []byte) {
	var msgs struct {
		Messages []struct {
			Message string `json:"message"`
			Level   string `json:"level"`
		} `json:"messages"`
	}

	err := json.Unmarshal(reply, &msgs)
	if nil != err {
		return
	}

	for _, msg := range msgs.Messages {
		switch strings.ToLower(msg.Level) {
		case "error":
			log.Errorf("%s", msg.Message)
		case "warn":
			log.Warnf("%s", msg.Message)
		case "info":
			log.Infof("%s", msg.Message)
		case "debug", "verbose":
			log.Debugf("%s", msg.Message)
		}
	}
}

func (p *Processor) processConnectAttempt(rep ConnectAttempt) {
	app := p.apps[rep.Key]
	if nil == app {
		return
	}

	if nil != rep.Err {
		switch {
		case collector.IsDisconnect(rep.Err):
			app.state = AppStateDisconnected
		case collector.IsLicenseException(rep.Err):
			app.state = AppStateInvalidLicense
		default:
			// Try again later.
			app.state = AppStateUnknown
		}

		log.Warnf("app '%s' connect attempt returned %s", app, rep.Err)
		return
	}

	app.connectReply = rep.Reply
	app.RawConnectReply = rep.RawReply
	app.state = AppStateConnected
	app.collector = rep.Collector

	app.HarvestTrigger = getHarvestTrigger(app.info.License, app.connectReply)

	log.Infof("app '%s' connected with run id '%s'", app, app.connectReply.ID)

	p.harvests[*app.connectReply.ID] = NewAppHarvest(*app.connectReply.ID, app,
		NewHarvest(time.Now()), p.processorHarvestChan)
}

type harvestArgs struct {
	HarvestStart        time.Time
	id                  AgentRunID
	license             collector.LicenseKey
	collector           string
	useSSL              bool
	rules               MetricRules
	harvestErrorChannel chan<- HarvestError
	client              collector.Client
}

func harvestPayload(p PayloadCreator, args *harvestArgs) {
	call := collector.Cmd{
		Name:      p.Cmd(),
		UseSSL:    args.useSSL,
		Collector: args.collector,
		License:   args.license,
		RunID:     args.id.String(),
		Collectible: collector.CollectibleFunc(func(auditVersion bool) ([]byte, error) {
			if auditVersion {
				return p.Audit(args.id, args.HarvestStart)
			}
			return p.Data(args.id, args.HarvestStart)
		}),
	}

	reply, err := args.client.Execute(call)

	// We don't need to process the response to a harvest command unless an
	// error happened.  (Note that this may change if we have to support metric
	// cache ids).
	if nil == err {
		return
	}

	args.harvestErrorChannel <- HarvestError{
		Err:   err,
		Reply: reply,
		id:    args.id,
		data:  p,
	}
}

func considerHarvestPayload(p PayloadCreator, args *harvestArgs) {
	if !p.Empty() {
		go harvestPayload(p, args)
	}
}

func harvestAll(harvest *Harvest, args *harvestArgs) {
	log.Debugf("harvesting %d commands processed", harvest.commandsProcessed)

	harvest.createFinalMetrics()
	harvest.Metrics = harvest.Metrics.ApplyRules(args.rules)

	considerHarvestPayload(harvest.Metrics, args)
	considerHarvestPayload(harvest.CustomEvents, args)
	considerHarvestPayload(harvest.ErrorEvents, args)
	considerHarvestPayload(harvest.Errors, args)
	considerHarvestPayload(harvest.SlowSQLs, args)
	considerHarvestPayload(harvest.TxnTraces, args)
	considerHarvestPayload(harvest.TxnEvents, args)
}

func harvestByType(ah *AppHarvest, args *harvestArgs, ht HarvestType) {

	// The collector may provide custom reporting periods for harvesting
	// TxnEvents, CustomEvents, or ErrorEvents.  As a result, this
	// function harvests by type.
	harvest := ah.Harvest

	// In many cases, all types are harvested
	//    at the same time
	//       at the same rate.
	// In such cases, harvest all types and return.
	if ht&HarvestAll == HarvestAll {

		ah.Harvest = NewHarvest(time.Now())
		go harvestAll(harvest, args)
		return
	}

	// Otherwise, harvest by type.  The first type is DefaultData.  This
	// comprises the Metrics, Errors, SlowSQLs, and TxnTraces whose
	// reporting periods have no custom reporting periods.
	if ht&HarvestDefaultData == HarvestDefaultData {

		log.Debugf("harvesting %d commands processed", harvest.commandsProcessed)

		harvest.createFinalMetrics()
		harvest.Metrics = harvest.Metrics.ApplyRules(args.rules)

		metrics := harvest.Metrics
		errors := harvest.Errors
		slowSQLs := harvest.SlowSQLs
		txnTraces := harvest.TxnTraces

		harvest.Metrics = NewMetricTable(MaxMetrics, time.Now())
		harvest.Errors = NewErrorHeap(MaxErrors)
		harvest.SlowSQLs = NewSlowSQLs(MaxSlowSQLs)
		harvest.TxnTraces = NewTxnTraces()
		harvest.commandsProcessed = 0
		harvest.pidSet = make(map[int]struct{})

		considerHarvestPayload(metrics, args)
		considerHarvestPayload(errors, args)
		considerHarvestPayload(slowSQLs, args)
		considerHarvestPayload(txnTraces, args)
	}

	// The next three types are those which may have individually-configured
	// custom reporting periods; they each may be harvested at different rates.
	if ht&HarvestCustomEvents == HarvestCustomEvents {
		log.Debugf("harvesting custom events")

		customEvents := harvest.CustomEvents
		harvest.CustomEvents = NewCustomEvents(MaxCustomEvents)
		considerHarvestPayload(customEvents, args)
	}

	if ht&HarvestErrorEvents == HarvestErrorEvents {
		log.Debugf("harvesting error events")

		errorEvents := harvest.ErrorEvents
		harvest.ErrorEvents = NewErrorEvents(MaxErrorEvents)
		considerHarvestPayload(errorEvents, args)
	}

	if ht&HarvestTxnEvents == HarvestTxnEvents {
		log.Debugf("harvesting transaction events")

		txnEvents := harvest.TxnEvents
		harvest.TxnEvents = NewTxnEvents(MaxTxnEvents)
		considerHarvestPayload(txnEvents, args)
	}
}

func (p *Processor) doHarvest(ph ProcessorHarvest) {
	app := ph.AppHarvest.App
	harvestType := ph.Type
	id := ph.ID

	if p.cfg.AppTimeout > 0 && app.Inactive(p.cfg.AppTimeout) {
		log.Infof("removing %q with run id %q for lack of activity within %v",
			app, id, p.cfg.AppTimeout)
		p.shutdownAppHarvest(id)
		delete(p.apps, app.Key())

		return
	}

	args := harvestArgs{
		HarvestStart:        time.Now(),
		id:                  id,
		license:             app.info.License,
		collector:           app.collector,
		useSSL:              p.cfg.UseSSL,
		rules:               app.connectReply.MetricRules,
		harvestErrorChannel: p.harvestErrorChannel,
		client:              p.cfg.Client,
	}

	go harvestByType(ph.AppHarvest, &args, harvestType)
}

func (p *Processor) processHarvestError(d HarvestError) {
	h, ok := p.harvests[d.id]
	if !ok {
		// Very possible:  One harvest goroutine may encounter a ErrForceRestart
		// before this.
		log.Debugf("unable to process harvest response %q for unknown id %q",
			d.Reply, d.id)
		return
	}

	app := h.App
	log.Warnf("app %q with run id %q received %s", app, d.id, d.Err)

	switch {
	case collector.IsDisconnect(d.Err):
		app.state = AppStateDisconnected
		p.shutdownAppHarvest(d.id)
	case collector.IsLicenseException(d.Err):
		// I think this is unlikely to ever happen (the invalid license
		// exception should trigger during the connect), but it is included
		// here for defensiveness.
		app.state = AppStateInvalidLicense
		p.shutdownAppHarvest(d.id)
	case collector.IsRestartException(d.Err):
		app.state = AppStateUnknown
		p.shutdownAppHarvest(d.id)
		p.considerConnect(app)
	case (d.Err == collector.ErrPayloadTooLarge) ||
		(d.Err == collector.ErrUnsupportedMedia):
		// Do not call the failed harvest fn, since we do not want to save
		// the data.
	default:
		d.data.FailedHarvest(h.Harvest)
	}
}

func NewProcessor(cfg ProcessorConfig) *Processor {
	return &Processor{
		apps:                  make(map[AppKey]*App),
		harvests:              make(map[AgentRunID]*AppHarvest),
		txnDataChannel:        make(chan TxnData, TxnDataChanBuffering),
		appInfoChannel:        make(chan AppInfoMessage, AppInfoChanBuffering),
		connectAttemptChannel: make(chan ConnectAttempt),
		harvestErrorChannel:   make(chan HarvestError),
		quitChan:              make(chan struct{}),
		processorHarvestChan:  make(chan ProcessorHarvest),
		appConnectBackoff:     AppConnectAttemptBackoff,
		cfg:                   cfg,
	}
}

func (p *Processor) Run() error {
	utilChan := make(chan *utilization.Data, 1)

	go func() {
		utilChan <- utilization.Gather(p.cfg.UtilConfig)
	}()

	for {
		// Nested select to give priority to appInfoChannel.
		select {
		case d := <-p.appInfoChannel:
			p.processAppInfo(d)
		case <-p.quitChan:
			return nil
		default:
			select {
			case d := <-utilChan:
				p.util = d
				utilChan = nil // We'll never check again.
			case <-p.quitChan:
				return nil
			case d := <-p.processorHarvestChan:
				p.doHarvest(d)

			case d := <-p.txnDataChannel:
				p.processTxnData(d)

			case d := <-p.appInfoChannel:
				p.processAppInfo(d)

			case d := <-p.connectAttemptChannel:
				p.processConnectAttempt(d)

			case d := <-p.harvestErrorChannel:
				p.processHarvestError(d)
			}
		}

		if nil != p.trackProgress {
			p.trackProgress <- struct{}{}
		}
	}
}

type AgentDataHandler interface {
	IncomingTxnData(id AgentRunID, sample AggregaterInto)
	IncomingAppInfo(id *AgentRunID, info *AppInfo) AppInfoReply
}

func integrationLog(now time.Time, id AgentRunID, p PayloadCreator) {
	if p.Empty() {
		return
	}
	js, err := IntegrationData(p, id, now)
	if nil != err {
		log.Errorf("unable to create audit json payload for '%s': %s", p.Cmd(), err)
		return
	}
	log.Infof("NR_INTEGRATION_TEST '%s' '%s'", p.Cmd(), js)
}

func (p *Processor) IncomingTxnData(id AgentRunID, sample AggregaterInto) {
	if p.cfg.IntegrationMode {
		h := NewHarvest(time.Now())
		sample.AggregateInto(h)
		now := time.Now()
		integrationLog(now, id, h.Metrics)
		integrationLog(now, id, h.CustomEvents)
		integrationLog(now, id, h.ErrorEvents)
		integrationLog(now, id, h.Errors)
		integrationLog(now, id, h.SlowSQLs)
		integrationLog(now, id, h.TxnTraces)
		integrationLog(now, id, h.TxnEvents)
	}
	p.txnDataChannel <- TxnData{ID: id, Sample: sample}
}

func (p *Processor) IncomingAppInfo(id *AgentRunID, info *AppInfo) AppInfoReply {
	resultChan := make(chan AppInfoReply, 1)

	p.appInfoChannel <- AppInfoMessage{ID: id, Info: info, ResultChan: resultChan}
	out := <-resultChan
	close(resultChan)
	return out
}

func (p *Processor) quit() {
	p.quitChan <- struct{}{}
}

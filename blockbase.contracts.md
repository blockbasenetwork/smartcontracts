<h1 class="contract">startchain</h1>

---
spec_version: "0.2.0"
title: Start BlockBase Chain
summary: 'Start the blockbase service'
icon:
---

The {{ owner }} inserts his public key and his account name as parameters. 

His account name will be used to identify his chain in the smart contract and his {{ publickey }} will be used to be visible to participants and to use futher down the line. The {{ owner }} and states data will be saved in a multi-index table, where the ram cost will be paid by the {{ owner }}.

<h1 class="contract">configchain</h1>

---
spec_version: "0.2.0"
title: Configure BlockBase Chain
summary: 'Configure the requested blockbase service,'
icon:
---

Insert the {{ owner }} and the {{ configurations }} json as parameter. 

The {{ owner }} name will used to identify his chain, in the smart contract and the json will be used to store the information in a multi-index table to be visible to all interested parties. The ram cost will be paid by the {{ owner }}.

<h1 class="contract">startcandtime</h1>

---
spec_version: "0.2.0"
title: Start Candidature time 
summary: 'Starts the period where candidates can join the chain'
icon:
---

Action triggered after the {{ owner }} configures the chain. 

The state will change from configure time to candidature time, allowing the interested producers to submit their candidature.

Launches a scheduled deferred transaction to be executed when the time defined in the configuration ends.

<h1 class="contract">secrettime</h1>

---
spec_version: "0.2.0"
title: Start Secret time 
summary: 'Starts the period where candidates must send their secret string'
icon:
---

Triggered when the cadidature time ends. Changes the states from candidature time to secret time, allowing the selected producers to send their secret to qualify for the productors selection.

When this action is executed, the candidates will be choosen and it will schedule a deferred transaction to be executed when the time defined in the configuration ends.

<h1 class="contract">startsendtime</h1>

---
spec_version: "0.2.0"
title: Start ip send time
summary: 'Starts the period where candidates must send their encrypted ip'
icon:
---

Triggered when the secret time ends and the transaction is executed. Changes the states from secret time to ip send time, allowing the selected producers to submit their encrypted ip addresses.

Launches a scheduled deferred transaction to be executed when the time defined in the configuration ends.

<h1 class="contract">startrectime</h1>

---
spec_version: "0.2.0"
title: Start ip recieve time
summary: 'Starts the period where candidates can check tables with encrypted ips and get ready for production'
icon:
---

Triggered when the ip send time ends and the transaction is executed. This action will change the states from ip send time to ip receive time, giving the producers time to decrypt the ips they must use and to execute the ready action making them ready for production. 

Launches a scheduled deferred transaction to be executed when the time defined in the configuration ends.

<h1 class="contract">prodtime</h1>

---
spec_version: "0.2.0"
title: Start production time
summary: 'Starts the sidechain production'
icon:
---

Triggered when the receive time ends and the transaction is executed. This action will change the states from ip receive time to production time, starting the service and allowing the producers to produce their blocks with the requested DB transactions.

The ram cost will be paid by the {{ owner }}.

<h1 class="contract">addcandidate</h1>

---
spec_version: "0.2.0"
title: Add candidate to chain
summary: 'Adds a candidate to the sidechain'
icon:
---

This transaction will insert or update the candidatures of the producers. This action will receive the {{ owner }}, the {{ candidate }}, the {{ worktimeinseconds }}, his {{ publickey }} and the {{ secrethash }}. It will also validate every parameter and only then, if the candidature is valid, it will insert the values into the table. This action is only available after the candidature time is started for the first time. The ram cost will be paid by the {{ candidate }}.

<h1 class="contract">addencryptip</h1>

---
spec_version: "0.2.0"
title: Add encrypted ip
summary: 'Inserts encrypted ips by the producer'
icon:
---

This action allow the producer to insert the ecrypted ips in the dedicated multi index table. This action will receive the {{ owner }}, {{ producer }}, and a list of encrypted ip's to submit. It also will validate every paramenter and only then, if valid, it will insert the values in the multi index table. The inserted list of encrypted ips will allow the other producers to decrypth and obtain the other producer ip for connection proposes. The ram cost will be paid by the producer.

<h1 class="contract">addsecret</h1>

---
spec_version: "0.2.0"
title: Add secret
summary: 'Inserts producer's secret'
icon:
---

This action allow the candidates to insert the secrets in the candidate multi index table. This action will receive the {{ owner }}, the {{ producer }}, and a {{ secret }}. It also will validate the paramenters and only then, if valid, it will insert the secret in the multi index table. This secret will be used to randomly choose the candidates for the block production. The ram cost will be paid by the {{ producer }}.

<h1 class="contract">addblock</h1>

---
spec_version: "0.2.0"
title: Add Block
summary: 'Inserts sidechain block header'
icon:
---

This action will allow the selected producers to submit block headers to the smart contract. This action will receive the {{ owner }}, the {{ produer }} and a {{ block }} as a json. This action will validade the block and all the data submited by the {{ producer }}, and only after the validation will the data be inserted in to the block multi-index table that will be paid by the {{ producer }}. 

<h1 class="contract">rcandidate</h1>

---
spec_version: "0.2.0"
title: Remove Candidate
summary: 'Remove candidate from sidechain'
icon:
---

This action will allow the user to remove is candidature from the sidechain. This action will receive the {{ owner }} and the {{ producer }}. This action will only allow the non selected {{ candidate }} to remove his candidature. Also it will validate the paramenters receive and only them it will save them in the multi index table.

<h1 class="contract">resetreward</h1>

---
spec_version: "0.2.0"
title: Reset Reward
summary: 'Reset candidate reward'
icon:
---

This action will be responsible for the rewards reset. It will receive the {{ owner }} and the {{ producer }} and will be called by the token account This action will reset the reward from a {{ producer }} by modifying the multi index table. 

<h1 class="contract">blistremoval</h1>

---
spec_version: "0.2.0"
title: Black List Removal 
summary: 'Remove producer from sidechain blacklist'
icon:
---

This action will allow the {{ owner }} to remove a {{ producer }} from the blacklist of his sidechain. This action will receive the {{ owner }} and the {{ producer }}, beeing only able to be executed by the {{ owner }} This action will check if the {{ producer }} is eligeble for removal from the blacklist and them, if verified, he will be removed from the multi index table. Beeing the ram payer of this action the {{ owner }}.

<h1 class="contract">iamready</h1>

---
spec_version: "0.2.0"
title: Ready check
summary: 'Enables producer to signal they are ready for production'
icon:
---

This action will allow the {{ producer }} to notify the contract that he is ready to produce blocks. This action will receive the {{ owner }} and the {{ producer }} checking if all is right for the block production. The {{ producer }} won't be required to produce blocks, unless if he is ready. The Ram cost of this action will be paid by the {{ producer }}.

<h1 class="contract">exitrequest</h1>

---
spec_version: "0.2.0"
title: Exit Request
summary: 'Enables producer request to leave sidechain'
icon:
---

This action allow the {{ producer }} to submit a request to leave the chain. This action will receive the {{ owner }} and the {{ producer }}. It will validate both the {{ owner }} and the {{ producer }}, and only if it is valid it will be executed and therefore the {{ worktimeinseconds}} will be updated for the next settlement.

<h1 class="contract">changecprod</h1>

---
spec_version: "0.2.0"
title: Change current producer
summary: 'Updates sidechain to current producer'
icon:
---

This action will be triggered when the deffered transction is executed. It will be responsible for the producer changing, for the state changing, the block count and when it is time for the settlement it will reward the producers, kick the bad producers and for all the block and block count managing. All the ram needed for the multi index table managing will be paid by {{ owner }}.  

<h1 class="contract">verifyblock</h1>

---
spec_version: "0.2.0"
title: Verify block header
summary: 'Allows confirmation of latest block header added to sidechain'
icon:
---

Action ran by a multi sig transaction sent by the producers to confirm that the block header is valid. This action will receive as paramenter the {{ owner }} of the chain, the {{ producer }} and the {{ blockhash }} of the submited block.

<h1 class="contract">endservice</h1>

---
spec_version: "0.2.0"
title: End Service
summary: 'Allows confirmation of latest block header added to sidechain'
icon:
---

This action will allow the {{ owner }} to end the service of the chain. This action enables a sidechain owner to delete the sidechain and clear the tables of their data. This action will receive and be paid by the {{ owner }}